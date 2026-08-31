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
  '{"end":{"line":1,"position":3},"file":"named.txt","lines":"cat\n","p":0,"q":2,"start":{"line":1,"position":1}}' \
  '{"end":{"line":2,"position":3},"file":"named.txt","lines":"cat","p":4,"q":6,"start":{"line":2,"position":1}}' > expected.jsonl
cmp actual.jsonl expected.jsonl || fail "named-file JSONL differs"

printf 'cat\n' | "${cgrep}" cat > actual.jsonl
[[ "$?" == 0 ]] || fail "standard-input search failed"
printf '%s\n' \
  '{"end":{"line":1,"position":3},"lines":"cat\n","p":0,"q":2,"start":{"line":1,"position":1}}' \
  > expected.jsonl
cmp actual.jsonl expected.jsonl || fail "standard-input JSONL differs"

printf 'a\0b\n' > binary.dat
expect_status 0 "${cgrep}" 'a\x00b' binary.dat > actual.jsonl
printf '%s\n' \
  '{"end":{"line":1,"position":3},"file":"binary.dat","lines":"a\u0000b\n","p":0,"q":2,"start":{"line":1,"position":1}}' \
  > expected.jsonl
cmp actual.jsonl expected.jsonl || fail "NUL JSONL differs"

printf '\377' > invalid.dat
expect_status 0 "${cgrep}" '\xff' invalid.dat > actual.jsonl
printf '%s\n' \
  '{"end":{"line":1,"position":1},"file":"invalid.dat","lines_base64":"/w==","p":0,"q":0,"start":{"line":1,"position":1}}' \
  > expected.jsonl
cmp actual.jsonl expected.jsonl || fail "Base64 JSONL differs"

printf 'café 中国 🤖\n' > utf8.txt
expect_status 0 "${cgrep}" '中国' utf8.txt > actual.jsonl
printf '%s\n' \
  '{"end":{"line":1,"position":12},"file":"utf8.txt","lines":"café 中国 🤖\n","p":6,"q":11,"start":{"line":1,"position":7}}' \
  > expected.jsonl
cmp actual.jsonl expected.jsonl || fail "UTF-8 JSONL differs"

printf 'a\nb\nc\nd\ne\n' > limit.txt
expect_status 0 "${cgrep}" '^.*$' limit.txt > actual.jsonl
printf '%s\n' '{"file":"limit.txt","p":0,"q":9}' > expected.jsonl
cmp actual.jsonl expected.jsonl || fail "default line limit differs"
expect_status 0 "${cgrep}" --lines=0 '^.*$' limit.txt > actual.jsonl
printf '%s\n' \
  '{"end":{"line":5,"position":2},"file":"limit.txt","lines":"a\nb\nc\nd\ne\n","p":0,"q":9,"start":{"line":1,"position":1}}' \
  > expected.jsonl
cmp actual.jsonl expected.jsonl || fail "unlimited line output differs"
expect_status 0 "${cgrep}" --raw 3 '^.*$' limit.txt > actual.jsonl
printf '%s\n' '{"file":"limit.txt","p":0,"q":9}' > expected.jsonl
cmp actual.jsonl expected.jsonl || fail "explicit raw limit differs"
expect_status 0 "${cgrep}" --raw=0 '^.*$' limit.txt > actual.jsonl
printf '%s\n' \
  '{"file":"limit.txt","match":"a\nb\nc\nd\ne\n","p":0,"q":9}' \
  > expected.jsonl
cmp actual.jsonl expected.jsonl || fail "unlimited raw output differs"

expect_status 0 "${cgrep}" --lines 1 --raw 2 --lines 4 cat named.txt \
  > actual.jsonl
printf '%s\n' \
  '{"end":{"line":1,"position":3},"file":"named.txt","lines":"cat\n","p":0,"q":2,"start":{"line":1,"position":1}}' \
  '{"end":{"line":2,"position":3},"file":"named.txt","lines":"cat","p":4,"q":6,"start":{"line":2,"position":1}}' > expected.jsonl
cmp actual.jsonl expected.jsonl || fail "last output policy did not win"

expect_status 0 "${cgrep}" --help > help.txt
grep -q -- '--lines' help.txt || fail "help omits line mode"
grep -q -- '--raw' help.txt || fail "help omits raw mode"
expect_status 2 "${cgrep}" --raw nope cat > actual.jsonl 2> error.txt

expect_status 1 "${cgrep}" dog named.txt > actual.jsonl
[[ ! -s actual.jsonl ]] || fail "no-match search produced output"

expect_status 2 "${cgrep}" '[' named.txt > actual.jsonl 2> error.txt
[[ ! -s actual.jsonl ]] || fail "invalid regexp produced output"
[[ -s error.txt ]] || fail "invalid regexp produced no diagnostic"

expect_status 2 "${cgrep}" cat missing named.txt > actual.jsonl 2> error.txt
printf '%s\n' \
  '{"end":{"line":1,"position":3},"file":"named.txt","lines":"cat\n","p":0,"q":2,"start":{"line":1,"position":1}}' \
  '{"end":{"line":2,"position":3},"file":"named.txt","lines":"cat","p":4,"q":6,"start":{"line":2,"position":1}}' > expected.jsonl
cmp actual.jsonl expected.jsonl || fail "search did not continue after error"
[[ -s error.txt ]] || fail "missing input produced no diagnostic"

exit 0
