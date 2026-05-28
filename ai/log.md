2026-03-09T01:09:21Z Initial repository reconnaissance: read `ai/architecture.md`, mapped top-level structure and core Cottontail/Meadowlark modules, and wrote baseline notes/code map to `ai/notes.md`.
2026-03-09T01:22:32Z Fixed standalone multithreaded TREC ranking helper in `src/ranker.cc` and adjusted `Stats::clone_()` in `src/stats.cc` so per-thread stats cloning works with started warrens.
2026-03-09T01:51:29Z Added minimal session-facing helpers to `Stats` (`end()` and `translate(...)`) and updated batch ranking code to use them instead of reaching through to `Stats::warren()`.
2026-03-09T02:18:56Z Expanded `ai/notes.md` with the agreed Warren/Meadow/Stats/Python MVP plan and updated `AGENTS.md` so work in `ai/` is generally permitted.
2026-03-09T02:24:04Z Replaced `apps/working.cc` with a disposable `rank`-style CLI that uses the new batch `trec(...)` helper and emits surrogate TREC scores from rank order.
2026-03-09T02:25:49Z Moved docno fallback probing into `src/ranker.cc::trec(...)` and simplified `apps/working.cc` to rely on the shared helper.
2026-03-09T02:27:33Z Removed legacy docno fallback probing from `src/ranker.cc::trec(...)`; shared batch ranking now strictly relies on identifiers resolved by `Stats`.
2026-03-10T20:45:53Z Performed repository reconnaissance: re-read `ai/architecture.md`, reviewed `README.md`, and surveyed top-level, `src/`, `meadowlark/`, and `test/` layout to refresh the current code map.
2026-05-14T23:54:40Z Drafted `ai/plan.ai` for Hazel v1: a single-file, self-describing Fiver-to-Hazel conversion milestone.
2026-05-14T23:57:08Z Updated `AGENTS.md` to direct future agents to read `ai/plan.ai` when it exists.
2026-05-15T00:00:30Z Extended `ai/plan.ai` with a final `apps/fiver2hazel.cc` CLI step for converting Fiver pickle files using same-level `dna`.
2026-05-15T17:57:23Z Implemented Hazel v1 Fiver writer methods, added `apps/fiver2hazel.cc`, updated the app build target, and verified `bazel build //apps:fiver2hazel`.
2026-05-15T18:51:13Z Reworked Hazel writing to stream idx postings and compressed txt chunks directly to the shard, write per-blob directories at the end, patch blob headers, and verified `bazel build //apps:fiver2hazel`.
2026-05-15T19:10:35Z Added configurable Hazel text compression chunk size, defaulting to 64 KiB, and exposed `--chunk-size` in `apps/fiver2hazel`.
2026-05-15T19:12:41Z Updated `ai/plan.ai` with Hazel writer implementation notes for activation: Bigwig-controlled started Fivers, stream-and-patch blob layout, singleton postings, text chunk directory, and text lookup via `text_chunk_tag`.
2026-05-15T23:59:56Z Added `ai/hazel.md` documenting the Hazel v1 file format, blob headers/directories, text chunking, and activation notes.
2026-05-16T12:22:05Z Replaced the completed Hazel writer plan with a static shard activation plan centered on a reusable magic/DNA/blob envelope and dispatch by `warren` DNA.
2026-05-16T12:26:00Z Updated Hazel/static shard writing to terminate the DNA section with a blank line before the top-level blob dictionary.
2026-05-16T13:07:50Z Completed single-file burrow dispatch in `Warren::make(...)` and added inert Hazel/HazelIdx/HazelTxt activation stubs.
2026-05-16T13:14:36Z Changed single-file burrow opening to read `#COTTONTAIL\n` as fixed magic bytes before line-reading DNA.
2026-05-16T13:27:57Z Moved Hazel's local name/recipe extraction helper into `recipe.*` as a cooked-parameter-map overload.
2026-05-16T16:01:09Z Added `ai/improvements.md` for potential cleanups and recorded the open `Txt::wrap(...)` design question.
2026-05-16T16:04:54Z Added temporary Hazel activation DNA dump to stderr for short-term `apps/fluffy` testing.
2026-05-16T16:07:26Z Corrected `ai/notes.md` to record Makefile targets as the preferred wrapper for consistent Bazel builds/tests.
2026-05-16T16:45:22Z Added reusable single-file burrow envelope and current Hazel stub notes to `ai/hazel.md`, then replaced `ai/plan.ai` with the Featurizer name/recipe round-trip side quest plan.
2026-05-16T20:07:07Z Changed Featurizer identity to virtual per-class names, made vocab pass through wrapped identity for persistent DNA, fixed tagging/json wrapper round-trips, and verified `make testing`.
2026-05-16T20:17:02Z Added compositional featurizer round-trip tests for nested tagging, JSON wrapping tagging, and tagging wrapping JSON; reverified `make testing`.
2026-05-16T20:28:22Z Renamed `ai/plan.ai` to `ai/plan.md` and updated current agent documentation references.
2026-05-16T20:30:03Z Removed completed `ai/plan.md` after finishing the Featurizer name/recipe round-trip side quest.
2026-05-16T20:43:33Z Recorded restart notes for completed Featurizer identity work; user verified Hazel dumped DNA error is gone.
2026-05-17T15:58:51Z Revised Hazel v1 writer format: idx directories now use boundary triples with inline token singleton overloads, txt directories now use raw/compressed boundary pairs, updated `ai/hazel.md`, and verified `bazel build //apps:fiver2hazel`.
2026-05-17T20:35:26Z Implemented first Hazel activation pass: real idx/txt blob parsing, no-cache locked idx reads, cached decompressed txt chunks, shallow Hazel cloning, compressed posting blob decoding, fixed the Hazel DNA separator, preserved owner Warren parameters in `fiver2hazel`, and verified `make testing` plus `bazel build //apps:fiver2hazel //apps:working`.
2026-05-17T23:21:14Z Added `ai/plan.md` for the Meadow/Hazel ranking metadata issue: `TfIdfStats::make(...)` currently requires a Warren-global `stemmer` parameter write, which conflicts with immutable Hazel shards and the desired database-scoped metadata direction.
2026-05-18T14:17:55Z Disabled the transitional `TfIdfStats::make(...)` Warren-global `stemmer` parameter write under `#if 0`, keeping the comment inside the disabled block, and verified `bazel build //apps:working`.
2026-05-18T14:44:35Z Replaced `ai/plan.md` with an unfinished Hazel efficiency planning sketch, updated `ai/hazel.md` to reflect working first-pass activation and current efficiency work, and refreshed `ai/notes.md` with the threaded BM25 stress-test bottlenecks.
2026-05-18T15:09:17Z Added `ai/hazel-progress.md` to track Hazel performance milestones; recorded the first successful standalone-Hazel `rank.sh` run with matching MRR and 43:36 wall time, and linked the progress log from `ai/plan.md` and `ai/notes.md`.
2026-05-18T15:10:33Z Clarified in `ai/hazel-progress.md` that the Fiver-backed wall time mostly reflects loading the Fiver into memory and that the hot internal ranking timer is the more meaningful comparison.
2026-05-18T16:20:48Z Added `src/read_gate.h` as the next Hazel efficiency starting point and documented it in `ai/plan.md`, `ai/hazel-progress.md`, and `ai/notes.md`; the helper is not yet wired into Hazel.
2026-05-19T15:01:56Z Adjusted `Fiver::hazel` txt chunk map writing so txt directory offsets and compressed chunk ends are relative to the start of chunk space after the txt header.
2026-05-19T15:44:36Z Simplified Hazel txt activation for the chunk-space-relative format: removed cross-chunk token-count skipping, kept at most two one-chunk tokenizer skips, and made `target_chunk_size` load-time validation only.
2026-05-19T16:16:57Z Stripped `HazelTxt` in `src/hazel.cc` down to placeholder methods and minimal wiring so the text activation path can be rebuilt from a clean skeleton.
2026-05-19T16:34:51Z Made `HazelTxt::clone_()` assert false, set an error, and return `nullptr`, documenting the intended non-cloning component model for Hazel text.
2026-05-19T16:42:30Z Rewired Hazel assembly so `Hazel::make` constructs the `text_chunk_tag` hopper and passes it into `HazelTxt`, removing HazelTxt's stored idx and featurizer dependencies.
2026-05-19T16:54:39Z Changed `HazelTxt` construction to take the Hazel filename plus txt blob offset/length, create its own `ReadGate`, and keep blob length scoped to the future map-loading step.
2026-05-19T17:03:39Z Folded Hazel txt map loading into `HazelTxt::make`: it now reads the txt header, sets `offset_` to the absolute start of chunk space, loads and validates the chunk map, and sizes the fixed cache vector.
2026-05-20T15:34:52Z Updated `ai/hazel.md` to document that txt `directory_offset` and directory `compressed_end` values are relative to chunk space after the txt header.
2026-05-20T16:42:18Z Reworked `ai/plan.md` into a HazelTxt rebuild plan and sanity-checked it against the writer format, hopper semantics, tokenizer skipping, compressor APIs, and cache representation constraints.
2026-05-20T18:58:29Z Performed repository reconnaissance: re-read architecture and current HazelTxt rebuild plan, surveyed top-level files/build setup, inspected Hazel activation stubs, ReadGate, and current notes.
2026-05-20T19:07:30Z Rebuilt `HazelTxt` activation and translation in `src/hazel.cc`: txt header/map validation, token range discovery, ReadGate-backed decompression cache, and Fiver-style token-to-byte translation; verified `bazel build //...`.
2026-05-20T19:10:12Z Fixed HazelTxt compressor recipe activation to use the writer's `compressor` / `compressor_recipe` txt keys; reverified `bazel build //apps:fiver2hazel //apps:working` and `bazel build //...`.
2026-05-20T20:00:58Z Added the post-HazelTxt-rebuild `rank.sh` timing/correctness result to `ai/hazel-progress.md`: same MRR/queries as baseline, 2566150 ms internal timer, 43:17.83 wall time.
2026-05-20T20:07:03Z Moved durable HazelTxt rebuild details into `ai/hazel.md`, refreshed Hazel notes, and reduced `ai/plan.md` to the next goal: plan HazelIdx caching.
2026-05-21T00:16:46Z Changed `HazelTxt::load_token_range()` from `bool` to `void` because it only derives cached range state and has no error path; verified `bazel build //apps:working`.
2026-05-21T19:54:37Z Replaced the HazelIdx caching placeholder in `ai/plan.md` with the agreed implementation plan: CacheRecord-backed Hazel cache, ReadGate loading, SimplePostingFactory cache decode, async hopper fills, and future Bigwig merged-cache direction.
2026-05-21T19:58:59Z Added a future improvement note for deep internal error logging, including Hazel cache read/decode failure visibility when assertions are disabled.
2026-05-21T21:21:54Z Re-read `ai/architecture.md` and the current HazelIdx cache loading plan, then inspected HazelIdx, SimpleIdx cache loading, ArrayHopper cache records, ReadGate, and SimplePosting compressed-blob decode paths before implementation.
2026-05-21T21:27:07Z Implemented the first HazelIdx decoded posting cache, added SimplePostingFactory cache-line fill from compressed blobs with error returns, adjusted cache-backed ArrayHopper to allow singleton lists, and verified `bazel build //apps:working`.
2026-05-21T21:33:11Z Recorded the first post-HazelIdx-cache `rank.sh` result in `ai/hazel-progress.md`: matching MRR/queries, 12794 ms internal timer, and 0:43.78 wall time.
2026-05-21T21:43:02Z Increased HazelIdx `ReadGate` reader count from 4 to 16 for the next utilization experiment and verified `bazel build //apps:working`.
2026-05-21T21:46:01Z Increased HazelTxt `ReadGate` reader count from the default 2 to 16 for the next utilization experiment and verified `bazel build //apps:working`.
2026-05-21T21:46:34Z Recorded the HazelIdx-only 16-reader `rank.sh` result in `ai/hazel-progress.md`: matching MRR/queries, 11850 ms internal timer, and 0:42.91 wall time.
2026-05-21T21:48:56Z Recorded the Hazel read-gate 16/16 `rank.sh` result in `ai/hazel-progress.md`: matching MRR/queries, 12111 ms internal timer, and 0:43.30 wall time on a noisy host.
2026-05-21T21:53:51Z Updated `ai/notes.md` with the current Hazel cache/read-gate status and replaced `ai/plan.md` with a next-step Hazel regression test plan that requires user confirmation before coding.
2026-05-21T22:01:45Z Recorded the Hazel read-gate 16/16 `rank.sh` result with 128 ranker threads: matching MRR/queries, 11801 ms internal timer, 0:42.88 wall time, and little change versus 54 threads.
2026-05-21T22:41:51Z Recorded the Hazel read-gate 16/16 `rank.sh` result with 1 ranker thread: matching MRR/queries, 172635 ms internal timer, 3:23.65 wall time, and 101% CPU.
2026-05-22T15:54:18+00:00 - Moved current plan.md to hazel-testing.md and created a new plan.md for planning to merge Hazels.
2026-05-24T07:13:38+00:00 - Fixed `SimplePosting::invariants()` fostings size check to compare against postings size.
2026-05-24T08:18:10+00:00 - Rewrote `ai/plan.md` with the agreed on-disk Hazel merge design and validation rules.
2026-05-24T08:26:59+00:00 - Updated `ai/plan.md` with a Hazel merge implementation reading guide and clarified separator-newline handling.
2026-05-24T08:30:26+00:00 - Added the `Fiver::hazel(...)` trailing-separator prerequisite to the Hazel merge plan.
2026-05-25T00:54:49+00:00 - Implemented the first Working-based Hazel-to-Hazel merge primitive in `src/hazel.*`, added the `Fiver::hazel(...)` trailing separator write prerequisite, and verified `bazel build //...`.
2026-05-25T01:33:16+00:00 - Reworked the Hazel merge implementation around input/output merge state structs with each input Hazel opened once; verified `bazel build //apps:fiver2hazel //apps:working` and `bazel build //...`.
2026-05-25T21:51:32+00:00 - Added a Hazel text deletion improvement note covering logical translation semantics and future physical txt chunk reclamation.
2026-05-25T21:52:17+00:00 - Broadened the text deletion improvement note from Hazel-only to Fiver/Hazel/Bigwig merge semantics.
2026-05-25T21:54:57+00:00 - Added an exclusion merge semantics improvement note for possible outer interval merging of `null_feature` postings.
2026-05-26T01:16:04+00:00 - Documented the struct-based Hazel merge implementation in `ai/hazel.md` and added restart notes for the active Hazel/stemmer investigations.
2026-05-26T01:18:54+00:00 - Replaced the completed Hazel merge plan with a focused Meadowlark ranking stemmer bug plan.
2026-05-26T01:31:12+00:00 - Corrected stale `ai/hazel.md` language that said Hazel/Hazel merge was deferred; Bigwig integration remains future work.
2026-05-26T01:31:50+00:00 - Sanity-checked `ai/hazel.md` and refreshed stale activation/cache wording.
2026-05-26T03:09:24Z - Fixed Meadowlark tf-idf stats stemmer/tokenizer ownership so metadata-created processors initialize the base `Stats` state; added a top-level agent rule to run only compile/build checks unless runtime experiments are explicitly requested.
2026-05-26T03:50:00Z - Fixed the old Bigwig DNA stemmer loading shadowing bug so direct `Bigwig::make(...)` activation honors the `stemmer` parameter; verified `bazel build //apps:rank`.
2026-05-26T03:59:46Z - Removed the Meadowlark creation-time write of the Warren-global `container` parameter; verified `bazel build //apps:meadowlark //apps:forage //apps:rank`.
2026-05-26T05:19:23Z - Fixed a `TfIdfForager` null-check typo for `total_featurizer_` and made default `open_meadow(...)` delegate through the format-validating overload; verified `bazel build //apps:meadowlark //apps:forage //apps:rank`.
2026-05-26T05:24:13Z - Refreshed restart notes for the completed Meadowlark stemmer/container work and replaced `ai/plan.md` with a planning-only Hazel merge testing plan that requires explicit user approval before coding.
2026-05-26T05:52:08Z - Rewrote `apps/fiver2hazel.cc` as a burrow-directory converter: discover live Fiver shards, materialize matching Hazel shards, merge them into one final Hazel when needed, and verified `bazel build //apps:fiver2hazel`.
2026-05-26T06:47:58Z - Replaced the old `apps/working.cc` scratch-ranking app with `apps/scratch.cc`, a no-merge Bigwig text-file builder that annotates `line:` and `file:` spans; verified `bazel build //apps:scratch //apps:fiver2hazel`.
2026-05-26T07:21:40Z - Strengthened `ai/plan.md` and `ai/notes.md` verification guidance: agents should run compile/build checks only and must not run test cases, including `bazel test`, unless explicitly asked for that specific test run.
2026-05-26T07:32:40Z - Added null, real, and bad-compressor Hazel regression profiles and marked the dedicated Hazel test target small; verified `bazel build //test:hazel_test`.
2026-05-26T07:36:09Z - Narrowed the `*test*` ignore rule so top-level test-named files remain ignored while directories such as `test/` and their contents are visible to Git.
2026-05-26T07:44:03Z - Finalized Hazel documentation around current format, activation, merge, and regression coverage; removed stale Hazel testing plan state and replaced `ai/plan.md` with the next-step Bigwig integration discussion plan.
2026-05-26T07:53:51Z - Rewrote `ai/notes.md` to remove stale restart/test-planning notes and summarize the current repo map, Hazel state, verification rule, and Bigwig integration next step.
2026-05-26T07:55:09Z - Added an `ai/improvements.md` note to split aggregate tests into focused Bazel targets, using `//test:hazel_test` as the pattern.
2026-05-27T00:07:50+00:00 - Re-read the architecture, active Bigwig/Hazel integration plan, Hazel notes, Bigwig/Fluffle/Fiver/Hazel surfaces, build targets, and confirmed the worktree was clean before changing only this log entry.
2026-05-27T00:58:23+00:00 - Added `--convert` and `--merge` modes plus conversion/merge/total timing output to `apps/fiver2hazel`; `--convert` removes existing Hazels, `--merge` prunes sequence-covered merged Hazels before merging; verified `bazel build //apps:fiver2hazel`.
2026-05-27T01:54:05+00:00 - Recorded the 2026-05-27 `fiver2hazel a.meadow` conversion/merge timings and follow-up merged-Hazel `rank.sh` result in `ai/hazel-progress.md`.
2026-05-27T02:11:00+00:00 - Restored deleted `apps/working.cc` and its `//apps:working` Bazel target from the parent of commit `0f31f41`; verified `bazel build //apps:working`.
2026-05-27T02:21:43+00:00 - Rewrote `trec_docno(...)` to avoid per-call regex construction/replacement in the ranking output hot path; verified `bazel build //apps:working //apps:rank`.
2026-05-27T02:40:47+00:00 - Adjusted `apps/working.cc` thread handling to default to twice hardware concurrency, accept positive requested thread counts, cap them at twice hardware concurrency, and warn on stderr when an explicit request is capped; verified `bazel build //apps:working`.
2026-05-27T03:08:51+00:00 - Replaced `apps/rank.cc` with the `working`/`cottontail::trec(...)` driver, removed the temporary `apps/working.cc` source and `//apps:working` target, and verified `bazel build //apps:rank`.
2026-05-27T07:35:00+00:00 - Added a Hazel merge idx fast path that raw-copies unique, non-excluded posting byte ranges and preserves inline singleton directory entries without decode/re-encode; verified `bazel build //apps:fiver2hazel //test:hazel_test`.
2026-05-28T03:29:55+00:00 - Recorded the post-fast-path `fiver2hazel a.meadow` timing in `ai/hazel-progress.md`: Hazel merge improved from `611399` ms to `594594` ms.
2026-05-28T03:43:14Z - Added double-buffered `AsyncReader` and `AsyncWriter` classes in `src/async.*` with a focused `books.json` copy test target; verified `bazel build //test:async_test` and `bazel build //test:tests`.
2026-05-28T03:56:38Z - Wired `AsyncReader` into Hazel merge txt chunk copying and idx posting streaming while preserving random `read_at` for metadata/special pre-reads; verified `bazel build //apps:fiver2hazel //test:hazel_test //test:async_test` and `bazel build //test:tests`.
2026-05-28T03:59:45Z - Changed `apps/fiver2hazel` to keep existing exact per-Fiver Hazel shards during conversion and to preserve intermediate Hazel shards after merge; verified `bazel build //apps:fiver2hazel`.
2026-05-28T05:59:14Z - Documented the Hazel merge async I/O experiment in `ai/hazel-progress.md`: even on HDD it improved merge time by only about 1%, so it is not worth carrying forward without stronger evidence of an I/O bottleneck.
2026-05-28T06:18:51Z - Cleaned `ai/notes.md` so durable repo notes keep the useful `fiver2hazel` intermediate-Hazel behavior and refer to the failed async read-ahead experiment without listing async helpers as current infrastructure.
