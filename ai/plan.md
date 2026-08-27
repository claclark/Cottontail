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
user reports that `apps/trec-example`, the regression tests, and
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

## Active Direction

The current push is the preliminary indexed string-matching work recorded in
`ai/regex.md`. Steps 1 and 2 are implemented: feature `-1` is virtual universal
position evidence with zero count; the `HashingFeaturizer` boundary is fixed;
and append normalization is centralized with space, tab, carriage return, and
newline as separators. Agent verification was compile-only through
`bazel build //...`; user regression testing is pending.

Do not begin step 3, the `Tokenizer` count/bow/phrase interface change, without
review and explicit authorization. The Meadowlark file-oriented metadata work
remains complete, and the separate Python wrapper still follows later.

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
