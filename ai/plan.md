# Plan: Hazel Efficiency

## Status

Planning is not finished yet.

The current goal is to make Hazel efficient enough for serious threaded ranking
work. The first activation pass was deliberately simple and correct-first. It
can open a standalone Hazel shard and run queries, but a long `rank.sh` stress
test shows that the reference implementation is far too slow compared with the
Fiver-backed Meadow.

This plan is a starting sketch, not a final design.

Track concrete measurements and before/after outcomes in
`ai/hazel-progress.md`.

## Current Baseline

- Hazel idx activation loads the idx directory, but does not cache decoded
  postings.
- `HazelIdx::hopper(feature)` binary-searches the directory, reads the posting
  bytes under a shared file lock, decodes the compressed posting blob, returns
  a fresh hopper, and forgets it.
- Hazel txt activation keeps one persistent `text_chunk_tag` hopper and a
  simple cache of decompressed text chunks.
- `HazelTxt::translate(...)` currently serializes too much work behind one
  mutex.
- `Hazel::clone_()` is shallow, so cloned Warrens share the same `HazelIdx`
  and `HazelTxt`.

## Stress-Test Observations

The user's `rank.sh` script is the semi-serious test:

- It runs threaded BM25 over thousands of MARCO queries.
- It uses many threads (`--threads 54` in the current script).
- The Fiver-backed Meadow path is reasonably fast once loaded.
- The Hazel-backed path runs much longer, exposing the reference
  implementation's bottlenecks.
- The first successful Hazel run took `43:36.18` wall time while producing the
  same MRR as the Fiver-backed run; details are recorded in
  `ai/hazel-progress.md`.

Known or suspected bottlenecks:

- Over-serialization in `HazelTxt::translate(...)`.
- Repeated reading and decompression of idx entries in `HazelIdx::hopper(...)`.
- Shallow Hazel cloning means worker threads share one `HazelTxt` mutex and
  one text-chunk hopper.
- `HazelTxt::clone_()` exists, but `Hazel::clone_()` currently bypasses it by
  shallow-copying `txt_`.
- Per-thread `Stats` clones can stampede on the same metadata/stat hoppers when
  there is no idx cache.
- `text_chunk_tag` itself is decoded through Hazel idx; if txt clones each make
  their own text-chunk hopper, idx caching becomes even more important.
- BM25 may request related information separately (`rsj`, `tf_hopper`, id and
  container hoppers), so repeated feature lookups should avoid repeated decode.
- Missing-feature lookups should probably be cached too, since repeated query
  terms may not exist in the vocabulary.
- The shared `std::fstream` plus mutex is safe, but cold cache reads still
  serialize. Later designs may want positioned reads or per-loader descriptors.
- `HazelTxt::tokens_in_chunk()` currently falls back to token splitting in one
  path; this may be acceptable for now but should be checked if translate
  remains hot.
- The text chunk cache currently has no eviction. That is fine for early
  stress testing but eventually needs a memory budget.
- Output/id translation after ranking may use both idx hoppers and txt
  translation heavily; optimize this path as part of the full ranking workload,
  not just query evaluation.

## Likely Direction

First design an integrated cache story, then implement it incrementally.

Potential first moves:

- Add a Hazel idx decoded-posting cache keyed by feature.
- Use a cache record that supports in-flight loading so concurrent requests for
  the same feature wait instead of duplicating reads/decompression.
- Keep the file-read critical section short: copy compressed bytes while locked,
  then decompress outside the file lock.
- Decide whether the cached value should be `CacheRecord`, `SimplePosting`, or
  another hopper-ready representation.
- Make Hazel clones less shallow for txt: each clone should likely get its own
  `text_chunk_tag` hopper while sharing immutable txt metadata and the
  decompressed chunk cache.
- Split Hazel txt locking so hopper state, chunk-cache state, file IO, and
  decompression are not all serialized behind one broad mutex.

Open design questions:

- Should idx caches live per `HazelIdx`, per Bigwig/Meadow, or eventually in a
  shared cache across Fiver/Hazel shard types?
- How should cache keys identify a static shard, sequence range, and feature?
- What memory budget and eviction policy are appropriate for decoded postings
  and decompressed text chunks?
- How much should the first cache borrow from `SimpleIdx` versus introducing a
  small Hazel-specific cache that can later be generalized?
- How should future Bigwig activation choose and coordinate Fivers and Hazels
  for the same sequence range?

## Related Metadata Work

The ranking metadata transition remains relevant but is not the main efficiency
problem.

- `TfIdfStats::make(...)` now treats the ranking-view stemmer/tokenizer as
  `Stats` state.
- The older Warren-global `stemmer` write is disabled under `#if 0` as a
  transitional artifact.
- Long-term, Meadowlark conventions should make ranking views discoverable from
  in-index metadata rather than one global mutable parameter block.

## Validation

Use the user's `rank.sh` as the main stress test after each meaningful
optimization. Compare against the Fiver-backed `rank.sh a.meadow` run for both
correctness and rough performance.
