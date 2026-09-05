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

printf '%65534s' '' > bulk.txt
printf 'needle' >> bulk.txt
expect_status 0 "${cgrep}" --raw 0 needle bulk.txt > actual.jsonl
printf '%s\n' '{"file":"bulk.txt","match":"needle","p":65534,"q":65539}' \
  > expected.jsonl
cmp actual.jsonl expected.jsonl || fail "bulk-read boundary output differs"

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

printf 'ab_ababa' > springy.txt
printf '%s\n' \
  '{"file":"springy.txt","match":"aba","p":3,"q":5}' \
  '{"file":"springy.txt","match":"aba","p":5,"q":7}' > expected.jsonl
expect_status 0 "${cgrep}" --raw 0 aba springy.txt > actual.jsonl
cmp actual.jsonl expected.jsonl || fail "literal raw output differs"

# File buffers, delegated expressions, and streams must report the same bytes.
printf '%s\n' \
  '{"file":"springy.txt","match":"aba","p":3,"q":5}' \
  '{"file":"springy.txt","match":"aba","p":5,"q":7}' > expected.jsonl
expect_status 0 "${cgrep}" --raw 0 'a[b]a' springy.txt > actual.jsonl
cmp actual.jsonl expected.jsonl || fail "singleton-class literal output differs"
expect_status 0 "${cgrep}" --raw 0 'a.a' springy.txt > actual.jsonl
cmp actual.jsonl expected.jsonl || fail "buffer fallback output differs"
printf 'ab_ababa' | "${cgrep}" --raw 0 aba > actual.jsonl
[[ "$?" == 0 ]] || fail "raw stream search failed"
printf '%s\n' \
  '{"match":"aba","p":3,"q":5}' \
  '{"match":"aba","p":5,"q":7}' > expected.jsonl
cmp actual.jsonl expected.jsonl || fail "raw stream output differs"

expect_status 0 "${cgrep}" --raw 2 aba springy.txt > actual.jsonl
printf '%s\n' \
  '{"file":"springy.txt","p":3,"q":5}' \
  '{"file":"springy.txt","p":5,"q":7}' > expected.jsonl
cmp actual.jsonl expected.jsonl || fail "oversized buffer matches were lost"

printf '' > empty.txt
expect_status 1 "${cgrep}" --raw 0 cat empty.txt > actual.jsonl
[[ ! -s actual.jsonl ]] || fail "empty file produced a match"
printf '' | "${cgrep}" --raw 0 cat > actual.jsonl
[[ "$?" == 1 ]] || fail "empty stream status differs"

expect_status 0 "${cgrep}" --raw 0 'a\x00b' binary.dat > actual.jsonl
printf '%s\n' '{"file":"binary.dat","match":"a\u0000b","p":0,"q":2}' \
  > expected.jsonl
cmp actual.jsonl expected.jsonl || fail "buffer NUL output differs"
expect_status 0 "${cgrep}" --raw 0 '\xff' invalid.dat > actual.jsonl
printf '%s\n' '{"file":"invalid.dat","match_base64":"/w==","p":0,"q":0}' \
  > expected.jsonl
cmp actual.jsonl expected.jsonl || fail "buffer Base64 output differs"

expect_status 2 "${cgrep}" --raw 0 cat missing named.txt > actual.jsonl 2> error.txt
printf '%s\n' \
  '{"file":"named.txt","match":"cat","p":0,"q":2}' \
  '{"file":"named.txt","match":"cat","p":4,"q":6}' > expected.jsonl
cmp actual.jsonl expected.jsonl || fail "raw search did not continue after error"
[[ -s error.txt ]] || fail "raw input error omitted diagnostic"

expect_status 0 "${cgrep}" --help > help.txt
grep -q -- '--lines' help.txt || fail "help omits line mode"
grep -q -- '--raw' help.txt || fail "help omits raw mode"
grep -qE -- '--(springy|no-springy|no-match)' help.txt &&
  fail "help includes retired options"
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
