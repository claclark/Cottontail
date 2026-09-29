# Current Plan

## Working Rule

No coding without a concrete reason, discussion, and explicit confirmation
from the user. Documentation review or approval of one narrow change does not
authorize adjacent source changes.

Work on one agreed step at a time. Approval for one step does not authorize
starting a later step. Entries in `ai/improvements.md` are possibilities, not
active work, until the user promotes one into this plan.

If an agreed change appears to require a broader abstraction or materially
different design, stop and discuss it. Proposals for cleanup and refactoring
are welcome, but they also require discussion before implementation.

For the current Meadowlark push, the user makes all commits and runs the
regression tests. Agent verification is compile/build-only unless the user
explicitly requests a runtime experiment or test run.

## Current Checkpoint

The application reorganization is complete as of 2026-09-27. Current utilities
remain in `apps/`; historical experiments, SPLADE tools, and benchmarks are in
`expr/`; the paper's TREC-update and JSON-table programs are in `iirj/`.
Shared `collection.*` and `walk.*` helpers now live in `src/`. All 42 binary
targets are preserved under their respective packages. The full optimized
build passes; agent verification did not run regression tests.

The six direct side-effecting assertion cases have been corrected for
`NDEBUG` builds. Fiver publication and four text-recovery truncations use the
always-on `affirm` helper; Stats tokenizer setup uses normal error returns.
`open_meadow` also propagates a failed Warren open without dereferencing it.
This was a narrow fix, not a general assertion cleanup.

The Meadowlark JSONL, TSV, text, and code ingestion work is complete. Durable
database and metadata conventions are recorded in `ai/meadowlark.md`; the
model-facing database bootstrap guide is `ai/exploring-meadowlark.md`.

Restartable parallel Hazel merging and foreground Bigwig consolidation are
complete. `Bigwig::consolidate(...)` implements the offline operation,
`consolidate` delegates to it (`finish-merging` remains a compatibility
symlink), and the user has verified the work through
the full regression suite, large MS MARCO consolidation, repeated ranking, and
interrupted background-merge recovery. The implementation, benchmarks,
recovery behavior, and focused coverage are recorded in
`ai/consolidation.md`.

The follow-up Bigwig merge-publication cleanup is committed as `63d70b8`. The
user reports that `iirj/trec-example` (then in `apps/`), the regression tests, and
`./rank.sh a.meadow` all pass after that change.

The first narrow memory-pressure response is implemented for standalone
Hazels. `Warren::trim_memory()` is a public operation with a default no-op;
Hazel overrides it to clear the shared decoded-posting `OwslaCache`. It leaves
the Hazel text cache untouched. Repeated calls and calls through shallow Hazel
clones are semantically harmless. Agent verification was compile-only; the
user has since reported that the complete regression suite passes.

`Optimizer::estimate_memory(...)` now provides a deliberately rough preflight
estimate for a GCL string or parsed expression. It expands phrases,
deduplicates term features, and prices their full posting counts at three
address-sized fields per posting; a parse failure estimates zero. Agent
verification was compile-only; the user has since reported that the complete
regression suite, including the dedicated optimizer target, passes.

`ssr-server` now applies the agreed first-cut Linux admission policy when a new
query arrives. It trims all persistent collection Warrens above two-thirds RAM
occupancy and rejects queries whose combined estimate exceeds one eighth of
physical RAM. The checks fail open off Linux or when `/proc/meminfo` cannot be
read. Its snippet cover cap is now 512 tokens. This work supports the server for
the ClimbMix collection in TREC RAG 2026. Agent verification remained
compile-only; the user reports that the combined changes have been tested in
various ways and are ready to commit.

## Active Direction: Ship Meadowlark

As of 2026-09-29, the direction remains shipping Meadowlark. The regexp work is
paused, not a release prerequisite. Do not assume every item in improvements.md
or every experimental feature must ship first.

The user makes commits and runs the broader regression suite. No new code is
authorized by this checkpoint. The application reorganization and its directory
READMEs are complete; the current task is updating agent documentation.

The next feature discussed is SPLADE support over ordinary JSON records in the
same index. The user's data includes `docid`, `raw_text`, and `splade_vector`:
the intention is to derive SPLADE annotations and also forage the record text
with TF-IDF for BM25, without loading a separate text collection. The legacy
SPLADE programs now in `expr/` are to be replaced, not treated as the new
Meadowlark interface. No replacement design or implementation is yet approved.

For exceptionally large JSONL inputs, the agreed practical approach is to split
at record boundaries and append the parts as separate logical files. Readying
uncommitted Fivers does not release their bulk memory; rolling prepared
transactions were discussed but are not planned work. See `ai/meadowlark.md`.

The implemented Meadowlark ingestion, metadata, foraging, consolidation, and
recovery records above remain the starting point for release planning. The
Python wrapper and other follow-ups need explicit release-scope decisions;
they are not silently promoted into mandatory work.

## Parked Regexp Work

- N-gram tokenizer/featurizer, universal feature, append normalization, GCL
  literal-feature syntax, and basic byte-phrase matching are implemented.
- The reusable byte-NFA compiler/reference matcher and flat-file cgrep work.
  cgrep has typed lazy machine caches, buffer literal specialization, general
  Haystack fallback, and separate buffer/Haystack line engines.
- The user considers the matcher stable as far as testing and use establish.
  Keep the existing machine vector and runner code unchanged while collecting
  workloads.
- CGREP_LOG optionally records replayable cgrep arguments for agent searches.
  It is developer instrumentation documented only in ai/, with no logging
  when unset. The app and app-test targets compile; new logging runtime cases
  have not been run by the agent.
- Continue normal dogfooding during Meadowlark work. Once the collection is
  useful, return to indexed regexp execution, then add useful follow/union
  special cases to both the index and scanner.
- Dictionary-backed short matching, Warren phrase/regexp entry points,
  cross-line code ingestion, and exhaustive indexed literal testing remain
  unfinished. Their place in the regexp return plan is in regex.md.
- The frontier-based seeking discussion is preliminary, not a proved general
  algorithm. No follow decomposition or general springy-NFA runner was added.

Current implementation: [cgrep.md](cgrep.md).
Indexed foundations, return plan, and design questions: [regex.md](regex.md).
Search-tool limitations: [cgrep-improvements.md](cgrep-improvements.md).
Historical changes: [log.md](log.md).

## Completed Meadowlark Filename And Labeling Step

1. JSONL filename membership is now chunk-based. JSONL writes one `/.`
   envelope and one normalized-filename feature interval per nonempty worker
   transaction rather than one filename interval per `:` record. This preserves
   the important `(<< : filename)` query, which returns the ordinary objects
   from the named file.
2. Activity metadata is now outside file data containers. An `@` metadata
   record is not contained by `/.`; the canonical `/` filename is separate;
   and each nonempty data `/.` chunk contains one leading `//` filename and its
   data payload. Metadata, the canonical filename, and all data chunks form a
   coordinated recoverable commit set, subject to the short sequential
   visibility window recorded in `ai/improvements.md`.
3. New `/` and `//` filename text is framed with the internal JSON string
   tokens. This preserves leading `./` and `/` in display without changing the
   normalized filename feature. Restart recognition tolerates historical raw
   names. Tokenless files deliberately publish `/`, `@`, and `//`, but no
   address-dependent `/.`, `:`, or filename feature.
4. JSON handling now separates lossy arbitrary-interval display through
   `json_translate(...)` from validating full-value conversion through
   `json_convert(...)`; machine parsing uses the latter. The unused
   `Txt::raw(...)` interface has been removed.

The user authorized this package after the semantics discussion spanning
2026-08-20 and 2026-08-21. The first user regression run exposed eager trailing
spaces in display translation and a legacy-restart test that queried an old
read epoch. Both narrow corrections compile successfully. The user subsequently
tested the change in several ways, including against indices dating from 2022,
newer Hazel/Fiver indices, and a build from scratch, and reports that it looks
good. The existing 1.3 TB ClimbMix index also booted and passed extensive use
without observed problems. The commit remains with the user.

The broader long-running-server memory discussion remains deferred. Bigwig
trimming, Hazel text-cache eviction, and service pressure policy are preserved
in `ai/memory.md` for possible later return; they are not the current project.

## Completed File-Oriented Foraging Step

Implementation was authorized on 2026-08-22 and is complete. Its implemented
model and rationale are recorded in `ai/forager.md`. The logical file is the
unit for derived annotations; one global `(name, tag)` definition is separate
from per-file completion records; the query remains top-level; `Forager` is a
transaction-neutral interval worker; and older TF-IDF metadata remains
readable but cannot be extended by the current writer.

Focused cases cover immutable definitions, validation before publication,
default-tag and primary-record selection, multi-worker TSV file scoping and
aggregate placement, literal legacy TF-IDF ranking/refusal, restart/skip
behavior, and write-free NullForager transactions. The user reports that the
complete regression suite and additional tests pass. The MS MARCO build and
ranking path also work, with parallel worker readiness restoring forage time
from the observed 11:23 regression to 3:12.

## Completed Empty And Tokenless Fiver Readiness Step

Write-free and tokenless Fiver transactions now serialize a commit artifact.
Focused coverage exercises direct activation, empty/tokenless/tokenful flat and
tree merges, and empty/tokenless/mixed Fiver-to-Hazel conversion. Hazel text
serialization begins at raw byte zero when tokenless Fivers precede the first
token chunk, while retaining the later token-chunk anchor and normal dust
ownership semantics. The user reports that the full regression suite and
additional tests pass after these changes.
