# Goal: Plan a Hazel Regression Test

Do not start coding without discussing and confirming the exact test shape with
the user.

The HazelIdx decoded posting cache is implemented and fast enough for now.
Standalone Hazel rank.sh checks are correct and much faster than the original
activation path:

- `MRR @10: 0.18923028380406587`
- `QueriesRanked: 6980`
- HazelIdx 16-reader gate run: `11850` ms internal ranking timer
- HazelIdx/HazelTxt 16/16 run on a noisy host: `12111` ms internal ranking
  timer

The next coding step should be a Hazel regression test, not more tuning and not
Bigwig integration yet. Bigwig/Hazel integration remains the next larger design
milestone after the regression safety net exists.

## Test Intent

Add a small, durable regression test that proves a Hazel shard activated from a
Fiver preserves the important query behavior needed by ranking and future
Bigwig integration.

The test should focus on correctness and cache behavior, not performance.

Potential assertions to discuss before coding:

- Build a tiny Fiver-backed Warren in a temporary working directory.
- Add a small set of annotations/text that includes:
  - ordinary multi-posting features;
  - an inline singleton posting feature;
  - at least one valued annotation;
  - enough text/chunk structure to exercise HazelTxt translation.
- Materialize a Hazel shard from that Fiver.
- Open the Hazel shard as a standalone Warren.
- Compare selected `Idx` behavior between the source Fiver/Bigwig view and the
  Hazel view:
  - `count(feature)`;
  - `hopper(feature)` results through `tau`, `rho`, `uat`, `ohr`, `L`, and `R`;
  - singleton feature behavior;
  - absent feature behavior.
- Compare selected `Txt::translate(...)` ranges so the text activation path is
  covered too.
- Touch the same Hazel feature more than once so the decoded posting cache is
  exercised without requiring a performance assertion.

## Open Questions For The User

- Should this live in the existing aggregate `test/` target as a C++ unit test,
  or should it be a smaller CLI-style smoke test?
- Should the test write a real Hazel file via `Fiver::hazel(...)`, or use a
  checked-in/minimal fixture shard?
- How broad should the hopper comparison be in the first pass?

## Not In This Step

- No Bigwig/Hazel merged-cache implementation.
- No performance threshold in the regression test.
- No large MARCO/rank.sh test in CI-style test code.
- No new Hazel format changes unless the test exposes a correctness bug.
