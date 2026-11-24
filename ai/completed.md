# Completed Work

Record completed milestones here, with links to implementation notes and the
verification performed. Keep upcoming work in [plan.md](plan.md) and dated
activity in [log.md](log.md).

## Local Warren Activation

`Warren::local(wlock, error)` activates a directory burrow with a caller-held
lock. Path-based Warren and Bigwig factories use the same local activation
path. This lets a future server acquire the lock, publish its listener
address, and then activate using the same handle.

Each obtained or initialized WLock can claim one activation. The claim moves
the handle to `activating` under its status mutex. Success sets `activated`.
Failure sets `failed` and saves the error for any later call with that handle.
`Bigwig.LocalActivation` covers a caller-held lock, rejection of a second
activation, and a cached error after the underlying problem has been fixed.
All 60 targets compiled. The agent did not run the runtime cases.

## Directory Locking

Implemented on 2026-11-19. Directory-backed Bigwig activation, creation,
recreation, and foreground consolidation use `WLock`. A second opener
fails with an owner diagnostic when available. Clones and background merge
workers share the Fluffle and keep its lock alive. Standalone Hazel activation
opens a file directly. Legacy Simple activation remains unlocked.

`src/wlock.*` implements three shared-pointer factories:

- `obtain`: require a directory, acquire a persistent `burrow.lock` with
  nonblocking exclusive `flock`, and write ASCII `hostname/pid` to `burrow.pid`.
  An existing PID entry must be a regular file. PID opens refuse symlinks.
  On contention, wait one second only if the PID file is absent, then report
  its owner or a generic locked message. A missing lock file requires valid
  DNA before creation; legacy Simple burrows succeed without either lock file.
- `create(name, burrow, error)`: require a missing target or an empty directory,
  establish the lock file, then obtain it. This operation does not initialize DNA.
- `recreate(name, burrow, error)`: obtain an existing recognized burrow and refuse
  Simple. After ordinary `temp.*` cleanup, check the remaining entries before
  deletion. Refuse subdirectories, non-regular entries other than the reserved
  `meadowlark.sock` socket, and read-only regular files.
  Preserve `burrow.lock` and `burrow.pid`. A missing target uses `create`.

WLock owns the descriptor and its Working. It constructs Working after
acquisition or on the unlocked Simple path. Releasing the descriptor leaves
the lock and PID files in place. For an older directory, WLock validates DNA
before creating its first lock file. Valid DNA with a missing or empty Warren
type is treated as Simple. A lock file identifies a new burrow even before
DNA initialization. `name()` is empty if that burrow has no valid DNA.
`create` rechecks directory entries after acquisition to catch a
competing creator that populated the directory.

The WLock-taking Bigwig factories are private, with Warren as a friend.
Creation requires a lock in `created` or `recreated` state and marks it
`initialized` after writing DNA. DNA writes check close and rename and clean
up the temporary file on failure. Direct Bigwig creation and Warren creation
check the recipe before preparing the directory. Bigwig's public in-memory
factory is `Bigwig::make_in_memory(recipe, error)`. In-memory Bigwigs skip
Hazel conversion, so the conversion size cutoff does not exclude Fivers
from ordinary merges.

`Warren::make(burrow, error)` detects the storage type. The legacy named
activation overload accepts `"simple"`. Creation through
`Warren::make(name, recipe, burrow, error, recreate)` currently supports
`"bigwig"` and defaults `recreate` to false. An empty burrow path selects
`the.burrow`. Meadowlark uses `a.meadow` by default and exposes the opt-in
operation through `--recreate`. See [meadowlark.md](meadowlark.md).

`consolidate` owns the lock throughout shard recovery, conversion, and merging.
The superseded `fiver2hazel` and `merge-hazels` tools were removed.

All 60 targets compiled. The user reported successful basic locking checks
and follow-up testing. Coverage includes clone lifetime, old burrow
activation, bad recipe retries, contention with consolidation, recreation
preflight, PID symlinks, and failed DNA publication. The agent ran build
checks only.

## Deletions And Text Garbage Collection

Completed as of 2026-11-13. The three agreed diffs implement outer null-feature
merging, deletion-aware translation, and physical text reclamation during Hazel
construction and merging. They are no longer release todos. Compile checks
pass; the user has exercised the heavy insertion/deletion TREC demo with and
without compression, consolidation, and subsequent ranking/evaluation. The
compressed consolidated index reaches the same 0.2515 MAP as the uncompressed
one. All 50,000 ranked docids match the expected collection patterns, with no
duplicates within a topic. Empty text-block runs match the deleted earlier
disks and CR documents; their retained map entries are accepted residue.
Measurements and the verification checkpoint are in `ai/hazel.md`; the updated
Figure 7 and its data are in `iirj/trec-2026-11-13.{png,log}`.

1. Implemented: outer merging for `null_feature`, including initial deletion
   posting construction, in `src/simple_posting.*`. Raw annotations are sorted
   canonically by feature, then by `(p ascending, q descending)` for null and
   `(q ascending, p descending)` otherwise. Selection retains strictly increasing
   ends for null, starts otherwise. The four construction sites use this sort in
   Fiver, SimpleBuilder, and SimpleAnnotator. Ordinary features retain inner
   semantics, now correctly normalizing nesting, equal endpoints, and duplicates.
   Both merge queues retain newest-source values for exact interval duplicates;
   inputs remain oldest to newest. The append shortcut checks across empty
   sources. Outer merging retains maximal intervals, without coalescing overlap
   or adjacency. Tests cover selection against an independent containment oracle,
   staged merges, exclusion filtering, live snapshots, Fiver/Hazel conversion,
   mixed collections, consolidation, and reopening. Focused target:
   `//test:erasures_test`; existing posting tests remain in `//test:tests`.
   Per the user, old partial Hazel merge logs are out of scope. SimpleBuilder's
   existing cross-batch ordered-GC-list input contract is unchanged.
2. Implemented: Hazel/Bigwig `Txt::translate` requests that touch
   any deleted position return an empty string. Coverage includes boundary
   contacts, partial overlaps, requests spanning deleted intervals, unaffected
   requests, and consistency across relevant storage and snapshot paths.
   Physical text reclamation was kept in the third diff. Existing clamping is retained, but
   original requested bounds are checked for deletion intersection:
   `rho(p)` finds the first erasure ending at or after `p`; a finite result
   starting at or before `q` invalidates the whole translation. Hazel checks
   its local null posting; Bigwig checks its whole snapshot before delegating
   to shards. Each creates a null-feature hopper (EmptyHopper if absent).
   Existing shard text sharing and mutexes remain; private hoppers for text
   clones are deferred to the wishlist.
   FiverTxt does not check erasures because the user does not activate lone
   Fivers; Bigwig checks its snapshot before delegating. A follow-up normalizes
   Fiver's upper `maxfinity` bound to `maxfinity - 1`, matching SimpleTxt:
   the user's test run exposed the existing sentinel translation corner case.
   `minfinity` seeks use `minfinity + 1` to skip the virtual sentinel interval.
   Coverage in `//test:erasures_test` includes retained snapshots, compression,
   cross-shard erasures, pre-clamp checks, conversion/merging, reopening,
   consolidation, and concurrent translations through shared text objects.
3. Implemented: reclamation of whole compressed text blocks during Hazel
   creation and merging. After completing the index, null and text-chunk postings
   are used to derive conservative half-open deleted byte ranges from fully
   erased token chunks. A block's payload is omitted only when its entire raw byte range is
   reclaimable. All directory entries, logical byte offsets, text-chunk anchors,
   token addresses, and allocation high-water marks remain intact. A hole has
   positive raw length and zero compressed length; retained boundary blocks
   and map entries are accepted residue. Conversion skips compression, merging
   skips payload copying and carries existing holes forward. No-erasure
   merging avoids rereading the completed index. The only text-reader change
   is rejecting nonpositive raw or compressed lengths in `obtain()`.
   Focused coverage in `//test:erasures_test` inspects payload lengths and map
   entries, checks surviving translations, empty/tokenless text, standalone
   reopening, snapshots, repeated merges, and append addresses after full
   reclamation and consolidation. Posting-log recovery uses the completed
   output index for reclamation decisions, including recovered feature work.

## Other Implementation Checkpoints

The SSR web demo in `apps/ssr-web.py` provides a single raw JSON display with
query, next-result, and full-document actions, visible cover tags with bold
content, an editable
default query, and configurable heading information through `--info`.
Launch instructions are in `ssr-demo/README.md`. Python and embedded JavaScript
syntax checks pass. The earlier HTTP query/document check and the user's
interactive use are recorded in `ai/log.md`; no new runtime checks were run
during push preparation. Multi-client SSR server work remains deferred.

The first collection-build support script is implemented in
`ssr-demo/climbix.sh`. It accepts source and target directories, preserves the
twelve ClimbMix file groupings from the original collection script, and runs
`consolidate` after each `meadowlark` ingestion, finding both executables on
`PATH`. All missing meadows are created before ingestion; reruns reopen existing
meadows and delegate recovery to ingestion and consolidation. Usage is documented in the
demo README. Verification is
shell syntax and static grouping review only; no collection build was run.
Further demo support work needs discussion and authorization.

The application reorganization is complete as of 2026-09-27. Current utilities
remain in `apps/`; historical experiments, SPLADE tools, and benchmarks are in
`expr/`; the paper's TREC-update and JSON-table programs are in `iirj/`.
Shared `collection.*` and `walk.*` helpers now live in `src/`. The full
optimized build passed at the reorganization checkpoint.

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

## SPLADE Ranking

The first SPLADE implementation was authorized on 2026-09-29. The user
subsequently reported all eight regression targets passing and a full
MS MARCO run. The linked-list follow-up was implemented on 2026-09-30.
The user's full run took 529,972 ms with unchanged MRR@10, and subsequent
regression results show both SPLADE targets passing.
Measurements are in [splade.md](splade.md).

The first SPLADE implementation supports ordinary JSON records in the
same index. The user's data includes `docid`, `raw_text`, and `splade_vector`:
the existing JSON field annotations already carry the SPLADE weights under
`:splade_vector:<label>:`. No separate SPLADE annotation pass is needed. The
record text can also be foraged with TF-IDF for BM25, without loading a separate
text collection. The legacy SPLADE programs in `expr/` remain unchanged as
historical experiments; the current CLI is `apps/splade`.

### First SPLADE Pass: Implemented Interface

- `src/splade.cc` and `src/splade.h` provide exact document-at-a-time batch
  ranking analogous to `cottontail::trec`, with parallelism across queries.
- Queries map topic IDs to maps of literal labels (such as `##n`) to `fval`
  weights. Construct `:splade_vector:<label>:` internally; use the stored
  floating-point annotation values and associate fields with their enclosing
  containers. Return translated identifier intervals, as with `trec`.
- Required arguments: Warren, queries, a string-to-string parameters map,
  results. A convenience overload omits parameters and passes an empty map.
  Optional arguments are `error = nullptr`, `threads = 0`, `time = nullptr`.
- Parameters and defaults: `container` = `:`, `prefix` = `:splade_vector:`,
  `id` = `:docid:`, `depth` = `1000`. Container and ID are GCL queries;
  term feature labels are constructed directly, without parsing or stemming.
- Finite nonnegative weights are required. Only positive scores rank; ties
  prefer earlier container starts. Top-k storage is bounded by depth, without
  document-wide accumulators or approximate pruning. Missing IDs are omitted,
  like `trec`; there is no backfill beyond the selected top-k containers.
- Hopper nodes are owned by a fixed vector and threaded into a raw-pointer
  list ordered by current position. Score the prefix in the head's document,
  then advance and reinsert those nodes. Containers must not overlap; each
  vector label must have at most one single-position annotation per container.
  Other hoppers are untouched. No WAND or threshold-window pruning is used.
- `apps/splade.cc` reads JSONL `qid`/`splade_vector` records and converts them
  to maps. Both integer and floating-point numeric query values are accepted.
  It prints TREC output with synthetic rank-based scores, like `apps/rank`.
  The CLI defaults to depth 10; the library still defaults to 1000.
- SPLADE accepts `--burrow` and `--meadow`, with only `--burrow` in help.
  `apps/fluffy.cc` (also built as `inspect`) now accepts the same alias.
  General convention: `--meadow` may alias `--burrow`, not the reverse. Other
  existing apps were not changed in this pass.
- `src/ranker.h` now defaults `trec`'s error and threads arguments, preserving
  argument order and implementation.
- Focused library coverage is `//test:splade_test`; CLI coverage is
  `//test:splade_app_test`. Runtime verification remains with the user.
  Linked-list cases cover reordered fields, hopper exhaustion, scoped gaps,
  and exhaustive dot-product comparison with 35 query labels. The old
  overlapping-container case was removed to match the agreed contract.

## Indexed Regexp And cgrep Foundations

- N-gram tokenizer/featurizer, universal feature, append normalization, GCL
  literal-feature syntax, and basic byte-phrase matching are implemented.
- The reusable byte-NFA compiler/reference matcher and flat-file cgrep work.
  cgrep has typed lazy machine caches, buffer literal specialization, general
  Haystack fallback, and separate buffer/Haystack line engines.
- The user considers the matcher stable based on testing and use so far.
- CGREP_LOG optionally records replayable cgrep arguments for agent searches.
  It is developer instrumentation documented only in ai/, with no logging
  when unset. The app and app-test targets compile; new logging runtime cases
  have not been run by the agent.

Current implementation: [cgrep.md](cgrep.md).

## Meadowlark Filename And Labeling

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
without observed problems.

## File-Oriented Foraging

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

## Empty And Tokenless Fiver Readiness

Write-free and tokenless Fiver transactions now serialize a commit artifact.
Focused coverage exercises direct activation, empty/tokenless/tokenful flat and
tree merges, and empty/tokenless/mixed Fiver-to-Hazel conversion. Hazel text
serialization begins at raw byte zero when tokenless Fivers precede the first
token chunk, while retaining the later token-chunk anchor and normal dust
ownership semantics. The user reports that the full regression suite and
additional tests pass after these changes.
