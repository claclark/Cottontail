#!/usr/bin/env bash

set -u
set -o pipefail

cgrep="${TEST_SRCDIR}/${TEST_WORKSPACE}/apps/cgrep"
work="${TEST_TMPDIR}/cgrep"
mkdir -p "${work}"
cd "${work}" || exit 1

fail() {
  echo "cgrep app test: $1" >&2
  exit 1
}

expect_status() {
  expected="$1"
  shift
  "$@"
  actual="$?"
  if [[ "${actual}" != "${expected}" ]]; then
    fail "expected status ${expected}, got ${actual}: $*"
  fi
}

printf 'cat\ncat' > named.txt
expect_status 0 "${cgrep}" cat named.txt > actual.jsonl
printf '%s\n' \
  '{"file":"named.txt","match":"cat","p":0,"q":2}' \
  '{"file":"named.txt","match":"cat","p":4,"q":6}' > expected.jsonl
cmp actual.jsonl expected.jsonl || fail "named-file JSONL differs"

printf 'cat\n' | "${cgrep}" cat > actual.jsonl
[[ "$?" == 0 ]] || fail "standard-input search failed"
printf '%s\n' '{"match":"cat","p":0,"q":2}' > expected.jsonl
cmp actual.jsonl expected.jsonl || fail "standard-input JSONL differs"

printf 'a\0b\n' > binary.dat
expect_status 0 "${cgrep}" 'a\x00b' binary.dat > actual.jsonl
printf '%s\n' \
  '{"file":"binary.dat","match":"a\u0000b","p":0,"q":2}' \
  > expected.jsonl
cmp actual.jsonl expected.jsonl || fail "NUL JSONL differs"

printf '\377' > invalid.dat
expect_status 0 "${cgrep}" '\xff' invalid.dat > actual.jsonl
printf '%s\n' \
  '{"file":"invalid.dat","match_base64":"/w==","p":0,"q":0}' \
  > expected.jsonl
cmp actual.jsonl expected.jsonl || fail "Base64 JSONL differs"

printf 'café 中国 🤖\n' > utf8.txt
expect_status 0 "${cgrep}" '中国' utf8.txt > actual.jsonl
printf '%s\n' \
  '{"file":"utf8.txt","match":"中国","p":6,"q":11}' \
  > expected.jsonl
cmp actual.jsonl expected.jsonl || fail "UTF-8 JSONL differs"

long_match=""
for ((i = 0; i < 257; i++)); do
  long_match+="a"
done
printf '%s' "${long_match}" > long.txt
expect_status 0 "${cgrep}" '^a+$' long.txt > actual.jsonl
printf '%s\n' '{"file":"long.txt","p":0,"q":256}' > expected.jsonl
cmp actual.jsonl expected.jsonl || fail "default match limit differs"

printf 'abcdef' > limit.txt
expect_status 0 "${cgrep}" --max-match 3 '^.*$' limit.txt > actual.jsonl
printf '%s\n' '{"file":"limit.txt","p":0,"q":5}' > expected.jsonl
cmp actual.jsonl expected.jsonl || fail "explicit match limit differs"
expect_status 0 "${cgrep}" --max-match=0 '^.*$' limit.txt > actual.jsonl
printf '%s\n' \
  '{"file":"limit.txt","match":"abcdef","p":0,"q":5}' \
  > expected.jsonl
cmp actual.jsonl expected.jsonl || fail "unlimited match output differs"

expect_status 0 "${cgrep}" --help > help.txt
grep -q -- '--max-match' help.txt || fail "help omits match limit"
expect_status 2 "${cgrep}" --max-match nope cat > actual.jsonl 2> error.txt

expect_status 1 "${cgrep}" dog named.txt > actual.jsonl
[[ ! -s actual.jsonl ]] || fail "no-match search produced output"

expect_status 2 "${cgrep}" '[' named.txt > actual.jsonl 2> error.txt
[[ ! -s actual.jsonl ]] || fail "invalid regexp produced output"
[[ -s error.txt ]] || fail "invalid regexp produced no diagnostic"

expect_status 2 "${cgrep}" cat missing named.txt > actual.jsonl 2> error.txt
printf '%s\n' \
  '{"file":"named.txt","match":"cat","p":0,"q":2}' \
  '{"file":"named.txt","match":"cat","p":4,"q":6}' > expected.jsonl
cmp actual.jsonl expected.jsonl || fail "search did not continue after error"
[[ -s error.txt ]] || fail "missing input produced no diagnostic"

exit 0
