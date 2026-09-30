#!/usr/bin/env bash

set -euo pipefail

splade="${TEST_SRCDIR}/${TEST_WORKSPACE}/apps/splade"
meadowlark="${TEST_SRCDIR}/${TEST_WORKSPACE}/apps/meadowlark"
fluffy="${TEST_SRCDIR}/${TEST_WORKSPACE}/apps/fluffy"
cd "${TEST_TMPDIR}"

fail() {
  echo "splade app test: $1" >&2
  exit 1
}

reject() {
  if "${splade}" "$@" > rejected.out 2> rejected.err; then
    fail "accepted invalid input: $*"
  fi
  [[ -s rejected.err ]] || fail "missing error message: $*"
  [[ ! -s rejected.out ]] || fail "partial ranking on error: $*"
}

printf '%s\n' \
  '{"docid":"A","key":"altA","splade_vector":{"x":2,"##n":1},"v":{"x":4}}' \
  '{"docid":"B","key":"altB","splade_vector":{"x":1,"##n":4},"v":{"x":8}}' \
  > documents.jsonl
printf '%s\n' \
  '{"qid":"both","splade_vector":{"x":2,"##n":1.0}}' \
  '{"qid":"empty","splade_vector":{}}' \
  > queries.jsonl
"${meadowlark}" --meadow demo.meadow --create --jsonl documents.jsonl

"${splade}" --burrow demo.meadow --threads 1 queries.jsonl > burrow.run
"${splade}" --meadow=demo.meadow --threads=2 queries.jsonl > meadow.run
cmp burrow.run meadow.run
printf '%s\n' \
  'both Q0 B 1 2 cottontail' \
  'both Q0 A 2 1 cottontail' \
  'empty Q0 FAKE 1 1 cottontail' > expected.run
cmp burrow.run expected.run

"${splade}" --meadow demo.meadow --depth=1 --container=: \
  --id :key: --prefix :v: --verbose queries.jsonl > custom.run 2> verbose.log
printf '%s\n' 'both Q0 altB 1 1 cottontail' \
  'empty Q0 FAKE 1 1 cottontail' > expected.run
cmp custom.run expected.run
[[ -s verbose.log ]] || fail "missing verbose output"

"${splade}" --help > help.out 2> help.err
help=$(<help.err)
[[ "${help}" == *--burrow* ]] || fail "help omits --burrow"
[[ "${help}" != *--meadow* ]] || fail "help exposes alias"
# Inspect is a symlink to fluffy; verify its shared argument parser accepts
# both spellings without needing an interactive terminal.
"${fluffy}" --burrow demo.meadow </dev/null > fluffy-b.out
"${fluffy}" --meadow demo.meadow </dev/null > fluffy-m.out
cmp fluffy-b.out fluffy-m.out

reject --burrow demo.meadow --threads=-1 queries.jsonl
reject --burrow demo.meadow --depth=1x queries.jsonl
reject --burrow demo.meadow --container='(' queries.jsonl
reject --burrow demo.meadow --unknown value queries.jsonl
reject --burrow nonexistent.meadow queries.jsonl
reject --burrow demo.meadow nonexistent.jsonl
for invalid in \
  '{' \
  '{"qid":42,"splade_vector":{}}' \
  '{"qid":"q","splade_vector":[]}' \
  '{"qid":"q","splade_vector":{"x":"bad"}}' \
  '{"qid":"q","splade_vector":{"x":-1}}'; do
  printf '%s\n' "${invalid}" > invalid.jsonl
  reject --burrow demo.meadow invalid.jsonl
done
printf '%s\n' '{"qid":"q","splade_vector":{}}' \
  '{"qid":"q","splade_vector":{}}' > duplicate.jsonl
reject --burrow demo.meadow duplicate.jsonl

: > empty.jsonl
"${splade}" --burrow demo.meadow empty.jsonl > empty.run
[[ ! -s empty.run ]] || fail "empty batch produced output"
