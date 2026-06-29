# Current Plan

No coding without discussion and confirmation from the user.

The Meadowlark JSONL append step discussed on 2026-06-28 is implemented and
logged.

Current implementation state:

- `apps/meadowlark.cc` parses input arguments into typed file records, starts
  the Warren once for preflight, checks every file with the public scalar
  `meadowlark::already_appended(...)`, flags existing sources, ends the Warren,
  and then dispatches only missing appends in command-line order.
- `append_jsonl(...)` no longer performs its own restart check. The CLI-level
  preflight avoids repeated start/check costs during large batch appends.
- JSONL source marker writing is private to `meadowlark.cc`. The helper owns
  filename normalization, marker text append, `"/"` marker annotation, and
  returned source-feature calculation.
- `append_jsonl(...)` wraps the main Warren as `path_scribe`, readies the path
  marker transaction, appends `path_scribe` to the worker Scribe vector after
  clone setup, and commits/finalizes/aborts through that one vector.
- `append_tsv(...)` preserves current behavior by explicitly committing after
  the ready-only Warren path marker wrapper.
- Verified compile-only builds: `bazel build //apps:meadowlark` and
  `bazel build //src:cottontail`.
- Bigwig merge selection no longer uses a split/barrier. `find_merge_action(...)`
  first asks `find_fiver_action(...)` for Fiver cleanup/conversion work, then
  falls back to `find_hazel_action(...)`.
- Fiver policy order is: lone-Fiver cleanup; whole-vector tiny Fiver run merge;
  oldest eligible stranded Fiver conversion; oldest eligible Fiver whose own
  estimate is at least `medium_shard`; smallest adjacent eligible Fiver pair
  where each side is below `medium_shard`.
- Current Bigwig thresholds are `small_shard` = 8 MiB, `medium_shard` = 256 MiB,
  and `large_shard` = 512 MiB. Verified compile-only builds after the policy
  work: `bazel build //src:cottontail` and `bazel build //apps:trec-example`.

Next possible work, after discussion:

- Move `append_tsv(...)` onto the same start/end, restart, verbose status, and
  batch commit pattern as JSONL while preserving current field/record
  annotations.
- Add JSONL-from-strings with an explicit source identity.
- Add raw text file append.
- Add code append as raw text plus metadata first.
- Revisit foraging after append operations share the same boring lifecycle.
- Delete `mrg.*` files during sanitization when appropriate.
