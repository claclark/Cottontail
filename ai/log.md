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
