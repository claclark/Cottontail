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
