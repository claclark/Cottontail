# Goal: Discuss Hazel Integration Into Bigwig

Hazel writer, activation, merge, scratch conversion utilities, and the dedicated
Hazel regression test are in place. The next step is not another Hazel
correctness test; it is a design discussion about how Hazel should join the
Bigwig/Fluffle infrastructure.

Do not start coding this integration before discussing the shape with the user.

## Current Hazel Position

- `Fiver::hazel(...)` can materialize a standalone Hazel shard from a built
  Fiver.
- `Warren::make(...)` can open a standalone Hazel as a Warren.
- `Hazel::merge(...)` can merge compatible Hazel shards into one canonical
  `hazel.<start>.<end>` shard.
- `apps/fiver2hazel` can convert live Fiver shards in a burrow into Hazel
  shards and merge them into a final Hazel for manual activation/testing.
- `test/hazel.cc` compares Fiver shard behavior against per-shard Hazel output
  and compares a no-merge Bigwig against the final merged Hazel under null,
  real, and bad compressor profiles.

## Discussion Targets

- When should Bigwig create Hazel shards from committed Fivers?
- Should Bigwig retain Fiver pickle shards after Hazel materialization, and for
  how long?
- When both `fiver.<start>.<end>` and `hazel.<start>.<end>` exist, should
  activation prefer Hazel by default?
- How should Fluffle represent live ranges if some are Fivers and some are
  Hazels?
- Should background compaction merge only Hazels, or should it also convert
  Fivers opportunistically before merge?
- What is the failure/publication protocol for background Hazel materialization
  and merge?
- What command-line/manual escape hatches should remain while this transition
  is experimental?

## Useful Starting Points

- `ai/hazel.md`
- `src/bigwig.h`
- `src/bigwig.cc`
- `src/fluffle.h`
- `src/fiver.h`
- `src/fiver.cc`
- `src/hazel.h`
- `src/hazel.cc`
- `apps/fiver2hazel.cc`
- `test/hazel.cc`

## Verification Rule

Agents should run compile/build checks only. Do not run test cases, including
`bazel test`, unless the user explicitly asks for that specific test run.
