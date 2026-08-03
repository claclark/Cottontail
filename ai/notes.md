# Cottontail Repository Notes

## Top-Level Layout

- `src/`: core Cottontail library. Public umbrella header is
  `src/cottontail.h`.
- `gcl/`: GCL query operators, S-expression parsing, legacy MT parser, and
  GCL-specific optimizer/hopper helpers. Built into the core Cottontail
  library.
- `meadowlark/`: Meadowlark layer built on top of the core library.
- `apps/`: CLI binaries, dataset utilities, and short-lived scratch programs.
- `test/`: Bazel C++ tests. `//test:tests` is the aggregate target;
  `//test:hazel_test` is the dedicated Hazel regression target.
- `ai/`: agent-facing architecture notes, plans, logs, and progress notes.
  `ai/consolidation.md` is the completed Bigwig/Hazel consolidation checkpoint;
  `ai/memory.md` is the deferred Warren memory-trimming design checkpoint.
- Local `*.burrow` and `*.meadow` directories are working indexes/examples and
  are ignored by Git.

## Core Architecture

- `Warren` (`src/warren.h`) is the central query/mutation interface. It owns or
  exposes `Working`, `Featurizer`, `Tokenizer`, `Idx`, `Txt`, `Annotator`, and
  `Appender`. Query access to `idx()` and `txt()` requires `start()`/`end()`.
- A clone of a started Warren should preserve the same read snapshot and come
  back started. Cloning does not clone an active write transaction; writers must
  call `transaction()` on their own clone.
- `Working` (`src/working.h`) abstracts filesystem storage, naming, temp files,
  readers/writers, and preload hints.
- `SimpleWarren` is the flat-file Warren implementation reconstructed from DNA
  metadata in a burrow.
- `Bigwig` is the dynamic Warren backed by `Fiver` shards and `Fluffle` state.
  Meadowlark creation goes through Bigwig.
- `Fiver` is the mutable/transaction shard format used by Bigwig.
- `Hazel` is the immutable single-file shard format produced from Fivers and
  opened as a standalone Warren.
- Major objects are intended to be safe for concurrent use unless documented
  otherwise. Hoppers are the important exception: they are stateful cursors
  and should be created and owned independently by each thread.

## Main Component Families

- Storage/indexing: `idx.*`, `simple_idx.*`, `simple_posting.*`, `txt.*`,
  `simple_txt*`, `fastid_txt.*`, `hazel.*`, `fiver.*`, `bigwig.*`.
- Ingestion/mutation: `builder.*`, `simple_builder.*`, `appender.*`,
  `annotator.*`, `scribe.*`.
- Query execution: `gcl/*`, `hopper.*`, `array_hopper.*`, `eval.*`,
  `ranking.*`, `ranker.*`.
- Text/feature processing: `tokenizer.*`, `ascii_tokenizer.*`,
  `utf8_tokenizer.*`, `featurizer.*`, `hashing_featurizer.*`,
  `json_featurizer.*`, `tagging_featurizer.*`, `vocab_featurizer.*`,
  `stemmer.*`, `porter.*`.
- Compression/stats/support: `compressor.*`, `post_compressor.*`,
  `tfdf_compressor.*`, `zlib_compressor.*`, `bad_compressor.h`, `stats.*`,
  `df_stats.*`, `idf_stats.*`, `field_stats.*`, `read_gate.h`.

## Current GCL Optimization Notes

- GCL query code lives in `gcl/` and remains part of the core Cottontail
  library through `//src:cottontail`.
- `gcl/optimizer.*` is on by default. `Optimizer::disable()` remains available
  for explicit comparisons, and `apps/ssr-server` exposes this through the
  `set_optimizer` protocol command.
- The old estimated-count rewrite for top-level
  `(<< (^ a b c ...) Q)` was removed. The current optimizer rule is narrower:
  entering `+` turns on a materialization context for its children; the first
  containment operator under that context (`<<`, `>>`, `!<`, or `!>`) is wrapped
  as `(materialize X)` and then treated atomically for further optimization.
  A nested `+` below that materialized node can start a new context.
- Phrase expansion produces containment (`>> (# n) (... ...))`, so a phrase
  branch under `+` is materialized at the phrase containment, not at the
  internal ordered-window node.
- Active `Materialize` fully enumerates the child hopper once with repeated
  `tau(p + 1)`. It preserves empty and singleton fast paths; for two or more
  postings it now pushes directly into `SimplePosting` and returns an
  `ArrayHopper` over that storage, avoiding the previous temporary vector plus
  shared-array copy. `SimplePosting` omits `q` storage when `q == p` and omits
  `v` storage while all values are `0.0`; those are semantic defaults restored
  by the hopper path.
- The sparse lazy-materialization map experiment remains behind disabled
  `COTTONTAIL_GCL_MATERIALIZE_LAZY` preprocessor branches for reference.
- `Hopper` accessor memoization now reuses cached answers across semantic
  intervals, not just exact keys: `tau` reuses when `p >= k >= cached-k`,
  `rho` when `q >= k >= cached-k`, `uat` when `q <= k <= cached-k`, and `ohr`
  when `p <= k <= cached-k`.
- `apps/ssr-server` serves JSON-line SSR requests over localhost for one or
  more burrows, and `apps/ssr-client` is the readline client for interactive
  query/next/full-document use. `apps/ssr-client.py` is the standard-library
  Python example client for the same protocol.
- `apps/ssr-timing` is a batch timing client for an existing `ssr-server`.
  Usage is `ssr-timing port timing.queries [seconds]`. Each query file row has
  a qid and a query. The client runs each query as `c/opt`, `w/not`, then
  `w/opt`, checks that returned docnos match, prints each timing as it arrives,
  appends flushed records to `timing.log`, and stops after a returned query
  exceeds the optional/default slow-query threshold.
- Informal reweighting of the stratified SSR timing sample suggested roughly a
  one-quarter reduction in average query time and removal of the extreme tail.
  Full materialization can still make some common phrases, notably
  `united states`, slower. See `ai/gcl-optimizer.md` for the measurement context.
- `SimpleIdx` posting-cache eviction is currently compiled out with
  `COTTONTAIL_SIMPLE_IDX_CACHE_EJECTION` set to `0`; cached postings remain for
  the life of the `SimpleIdx` unless the idx is reset or destroyed.

## Meadowlark Map

- `meadowlark/meadowlark.*` owns Meadowlark creation and format-specific file
  ingestion; `meadowlark/metadata.*` owns typed metadata creation and forager
  metadata parsing.
- `meadowlark::append_all(...)`, declared in `meadowlark/meadowlark.h`, owns the
  reusable typed input plan, duplicate preflight, and dispatch used by
  `apps/meadowlark`.
- `meadowlark/forager.*`: pluggable annotation passes over intervals or GCL
  query results.
- Current foragers include `tf-idf_forager.*` and `null_forager.h`.
- The durable format, metadata, provenance, and restart conventions are in
  `ai/meadowlark.md`. The concise model bootstrap is
  `ai/exploring-meadowlark.md`.

## CLI Surfaces

- `apps/meadowlark.cc`: create/open a meadow, parse `--tsv`, `--jsonl`/`--json`,
  `--text`, and `--code` inputs, and delegate the typed plan to the public
  `meadowlark::append_all(...)` library operation. Immediately after
  `--create`, checked `parameter:value` assignments may precede the first input
  flag; they are applied through `set_parameter(...)` before ingestion.
- `apps/forage.cc`: run a Meadowlark forager over a query; supports
  `--key value` and `--key=value` parameters.
- `apps/fluffy.cc`: interactive GCL query shell over a burrow or Hazel.
- `apps/rank.cc`: ranking CLI.
- `apps/ssr-server.cc`: localhost JSON-line SSR server over one or more
  burrows. It takes `[--fields fields] container content docno burrow...`,
  binds an automatic localhost port, and reports `listening on port N` for
  clients. Query results rank within `content`; `container` is used to find
  docno/document identity.
  Snippets stay within the ranked content interval, highlight with
  `<cover>...</cover>`, clip oversized covers to the first 200 tokens, and keep
  up to 1000 covers available through `next`. The optional `--fields` value is
  a comma-separated list of field GCL queries used only to assemble full
  `document` responses in caller-specified order, with outer quotes stripped
  from each translated field piece and pieces joined by ` ... `.
- `apps/ssr-client.cc`: readline client for `ssr-server`; non-empty input sends
  a query, empty input or `@next` requests the next result, and `@full` requests
  the full document for the last returned result.
- `apps/ssr-client.py`: no-dependency Python example client for the same
  JSON-line protocol and interactive `@next`/`@full` commands.
- `apps/simple.cc`: build a simple burrow from TREC/MARCO-style corpora.
- `apps/fiver2hazel.cc`: convert live Fiver shards in a burrow to Hazel shards
  with `--convert`, merge available Hazel shards with `--merge`, and time the
  conversion/merge phases. Existing exact per-Fiver Hazels are reused, and
  intermediate Hazels are preserved after merge. With no mode flags it does
  both phases.
- `apps/merge-hazels.cc`: explicitly merge a given ordered list of standalone
  Hazel files, require that they form a complete sequence, infer the sibling
  output Hazel name from the input sequence range, and delegate merge validation
  to `Hazel::merge(...)`.
- `apps/finish-merging.cc`: invoke offline `Bigwig::consolidate(...)` for each
  directory argument, skip regular-file arguments, and stop at the first
  failure. `--verbose` enables timestamped phase descriptions and timings from
  the consolidation operation.
- `apps/scratch.cc`: scratch utility for creating no-merge Bigwig/Fiver shards
  from small text files with `line:` and `file:` annotations.

## Build And Verification

- `MODULE.bazel` defines the Bazel module with `nlohmann_json`, `googletest`,
  and `rules_cc`.
- `src/BUILD` exports `//src:cottontail`, including `src/*`, `gcl/*`, and the
  Meadowlark library.
- `apps/BUILD` contains standalone `cc_binary` targets.
- `test/BUILD` contains aggregate `//test:tests` and dedicated
  `//test:hazel_test` and `//test:optimizer_test`.
- Repository rule: agents should run compile/build checks only. Do not run test
  cases, including `bazel test`, unless the user explicitly asks for that
  specific test run.

## Memory Trimming Direction

- The original query caches assume that an index generally fits in memory and
  behave as lazy-load-and-retain structures. Long-lived servers over larger
  ClimbMix indexes can therefore grow without bound.
- The current deferred direction is a public, thread-safe
  `Warren::trim_memory()` operation. It should release a substantial amount of
  reconstructible memory without changing semantics or durable state; roughly
  half is an aspiration rather than a total-memory guarantee.
- Process measurement, high-water admission control, polling, and restart
  policy belong to the surrounding service. It should pause new queries and
  trim every active Warren and clone before deciding whether to escalate.
- Bigwig trimming must cover both current Fluffle state and the historical
  cache/Owsla snapshot held by the particular started view. Current Fluffle
  population is not the same as all live query state.
- An in-place, internally locked `OwslaCache` clear is compatible with shared
  posting ownership and repeated calls. Hazel text chunks need a separate safe
  lifetime design because translation currently borrows raw pointers while
  copying decompressed bytes.
- Detailed reasoning and unresolved questions are in `ai/memory.md`. No
  implementation is authorized yet.

## Current Hazel Status

- Hazel v1 writer, activation, merge, and regression coverage are in place.
- Standalone Hazel files can be opened by `Warren::make(...)`, parse idx/txt
  blobs, construct hoppers from posting blobs, translate text through cached
  decompressed text chunks, and shallow-clone the Hazel Warren.
- Hazel sequence metadata is optional for standalone Hazel files. Activation
  validates it when present and caches `-1, -1` for `get_sequence(...)` when it
  is absent.
- Hazel idx activation uses `OwslaCache`-backed waitable `SimplePosting` cache
  entries. Non-inline posting hoppers share cached postings; query-time cache
  fills use a 16-reader `ReadGate`, run in a background thread, and publish
  completion through `SimplePosting::release()`. Hazel's concrete
  `posting(feature)` method fills synchronously on the caller's thread and
  returns a ready posting.
- Hazel txt activation loads the text map, uses a 16-reader `ReadGate`, keeps a
  mutex-protected `text_chunk_tag` hopper, and caches decompressed chunks
  without eviction.
- Started Hazel clones share the source `HazelIdx` and `HazelTxt` objects and
  therefore share both caches. Ending a clone does not release those components;
  cache memory remains until every owning Hazel Warren is destroyed.
- `Hazel::merge(...)` requires compatible compressors/DNA and increasing,
  non-overlapping input sequence ranges; gaps are allowed. It writes and
  publishes an activated but unstarted output Hazel. Working/name adapters
  preserve the existing bool-returning command surfaces.
- The canonical merge uses restartable
  `merge.<segment>.<sequence-start>.<sequence-end>` posting logs. Exactly two
  Hazels use one posting worker; three or more use `allowed_threads(0)`, capped
  by feature work. The durable segment count does not change on recovery.
- `null_feature` and adjusted text-chunk postings are prepared before ordinary
  workers. Workers dynamically claim features, merge source postings in
  chronological shard order, and append complete `SimplePosting` records.
- Recovery validates complete record prefixes, truncates a partial tail, and
  resumes missing features. Final assembly k-way merges the logs, omits empty
  completion markers, inlines eligible singletons, and copies other compressed
  records without recompression. Text chunks retain their compressed-copy
  merge.
- The final Hazel is assembled in its ordinary `.tmp` file and published
  before source or segment cleanup. No `mrg.*` marker is created. Legacy
  `mrg.*`, `pst.*`, and `dct.*` files are removed rather than resumed.
- Hazel merge async read-ahead was tried and measured on HDD, but the gain was
  only about 1%; the recommendation is not to carry that complexity forward
  without stronger evidence of an I/O bottleneck.
- `test/hazel.cc` is the completed Hazel regression test. It builds a no-merge
  Bigwig from small text files, converts each Fiver to Hazel, compares
  Fiver-vs-Hazel shard behavior, merges Hazels, and compares the merged Hazel
  against the source Bigwig with null, real, and bad compressor profiles. It
  also checks that started Hazel clones stay started and remain readable after
  the source Hazel is ended. Recovery coverage includes truncated/incomplete
  segment sets, aborted-transaction gaps, conflicting recoveries, legacy
  cleanup, and valid resume.
- The user reports that the full `make testing` regression run passes.
- Consolidated Hazel format, activation, merge, Bigwig integration, and
  performance-shape notes live in `ai/hazel.md`.

## Current Bigwig Status

- Fluffle owns the Bigwig merged-posting cache generation as an `OwslaCache`.
  `OwslaCache::get(feature, posting_factory, &fill)` atomically returns an
  existing waitable `SimplePosting` or installs a closed one and marks the
  caller as the fill owner.
- A started Bigwig read view captures both the visible shard vector and the
  current Fluffle cache pointer. Existing started readers keep their cache
  generation by `shared_ptr` lifetime.
- The cache generation is replaced when a kitten becomes a committed visible
  Fiver, inside `Bigwig::commit_()` while `fluffle_->lock` is held. Do not
  invalidate the cache in `ready()`: the shard is still a kitten there and is
  not part of the visible read snapshot.
- Background merge publication also replaces the Fluffle cache when the visible
  population reaches one Owsla. The Bigwig merged-posting path is bypassed for
  a one-Owsla view, so retaining that cache in the Fluffle would only prolong
  its lifetime. Already-started views keep their prior cache by shared
  ownership.
- Static indexes reuse the same cache generation across `end()` -> `start()`
  cycles until a visible commit changes the logical snapshot.
- Constructing a `Working` object now removes generic `temp.*` files, covering
  both new and existing burrows.
- Bigwig directory startup now runs `sanitize(...)` in `src/bigwig.cc`.
  Communication records `OwslaShard` and `HazelMergeRecovery` live in
  `src/owsla.h`. `Fiver::sanitize(...)` owns strict Fiver parsing,
  same-type contained-shard cleanup, and `kitten*` removal.
  `Hazel::sanitize(...)` owns strict Hazel parsing, same-type contained-shard
  cleanup, legacy sidecar cleanup, merge-segment grouping, and production of
  logical restartable Hazel merge records.
- Bigwig's sanitizer coordinator allows sequence gaps and mixed Hazel/Fiver
  order. A Fiver fully covered by one Hazel is deleted so the Hazel wins;
  Fivers containing Hazels, partial mixed overlaps, and same-type overlaps that
  cannot be explained by publication are rejected.
- A partial Hazel merge remains coherent only when its recorded sources match
  a consecutive set of sanitized Hazel shards. Invalid groups are removed. If
  recoveries overlap, every conflicting group is removed rather than choosing
  one.
- Bigwig startup activates sanitized shards in sequence order, adds them to the
  Fluffle visible list, and captures started read snapshots as
  `std::vector<std::shared_ptr<Owsla>>`.
- Fluffle owns the sanitized pending Hazel merge recovery list as
  `hazel_merges`; startup copies it from `SanitizedInventory`. Each recovery
  carries its target, source Hazel sequence, and durable segment count.
  Background selection prefers recovered Hazel work; worker preparation
  removes the accepted record and any conflicting segment groups before
  performing the action.
- `merge` and `convert` are live Fluffle parameters. Lock-held merge policy
  reads them directly and treats either as enabled when absent.
- `find_merge_action(...)` first checks `merge`, asks
  `find_fiver_action(...)`, and then falls back to
  `find_hazel_action(...)`. `merge:no` prevents selection of new work but does
  not interrupt an action already running.
- `convert:no` gates the three Fiver-to-Hazel selectors. Fiver/Fiver merging
  continues and ignores the normal 256 MiB per-input cap, allowing a burrow to
  converge to one large in-memory Fiver.
- `Bigwig::merge()` enables merging and conversion;
  `Bigwig::merge(false)` disables merging without changing `convert`; and
  `Bigwig::merge(true, false)` enables Fiver-only consolidation. Enabling
  merging calls `try_merge()` so the parameter change actively triggers work.
- Fiver policy order is: lone-Fiver cleanup; merge a run of at least three tiny
  eligible Fivers anywhere in the visible vector; convert the oldest eligible
  Fiver stranded between Hazels; convert the oldest eligible Fiver whose own
  estimate is at least `medium_shard`; merge the smallest adjacent eligible
  Fiver pair where each side is below `medium_shard`.
- Hazel policy continues recovered Hazel merges first, otherwise merges the
  smallest eligible adjacent Hazel pair. Hazel throttling applies only to Hazel
  actions.
- `BigwigIdx` composes postings from visible `Owsla` children. Its multi-shard
  path handles empty and single-shard cases directly, merges `text_chunk_tag`
  postings fresh without caching, and uses the captured `OwslaCache` only for
  true normal multi-shard posting merges.
- When deletions exist, `BigwigIdx` caches raw feature and `null_feature`
  merges separately and composes their hoppers with `NotContainedIn`.
- `CacheGate` is a one-way completion gate. `CacheRecord` starts closed and
  SimpleIdx cache-fill paths call `release()` after storage is filled.
  `SimplePosting` also carries a completion gate; normal postings are
  default-open, and deferred cache postings can be created closed and released
  after their vectors are filled.
- `ArrayHopper` supports raw-array, `CacheRecord`-backed, and
  `SimplePosting`-backed construction. Deferred backings are waited on and
  bound in out-of-line `ArrayHopper::bind()`; hopper-local `ready_` is plain
  because hoppers are thread-local cursors.
- `SimplePosting` is storage-only. It no longer has `hopper()` and no longer
  inherits from `enable_shared_from_this`; callers construct `ArrayHopper`s
  explicitly from known non-empty or deferred-known-non-empty postings.
- `Owsla` is the narrow Warren subclass for shards that expose
  `posting(feature)`, a cheap cached `estimated_size()`, sequence-range access,
  and `discard(...)`. Fiver and Hazel both subclass `Owsla`.
- Fiver's `estimated_size()` returns its existing logical storage estimate.
  Hazel caches its estimate at activation from loaded Hazel idx/txt directory
  metadata, avoiding filesystem access under the Fluffle lock.
- The old feature-level `Fiver::merge(...)` hopper helper has been removed.
  `Fiver::merge(...)` now refers only to physical Fiver-to-Fiver shard merge;
  BigwigIdx owns visible-read feature posting composition and caching.
- `FiverIdx::hopper_()` returns `SingletonHopper` directly for in-memory
  one-entry postings. Hazel still relies on `ArrayHopper` being correct for
  one-entry waitable `SimplePosting`s.
- Bigwig multi-Fiver cache misses now install the closed `OwslaCache` posting,
  start a detached fill thread, and return an `ArrayHopper` over the waitable
  posting. The fill thread captures the posting, factory, and contributing
  Fivers rather than the `BigwigIdx` object.
- Started Bigwig clones preserve the source read view without cloning an active
  write transaction; a clone that wants to write must call `transaction()`
  itself. Focused regression coverage checks that a started Bigwig clone stays
  readable after the parent ends and after the parent commits new content.
- `Bigwig::consolidate(...)` is the offline foreground path used by
  `finish-merging`. It shares DNA/component/sanitization setup with
  `Bigwig::make(...)` and allows aborted-commit gaps. It greedily splits each
  consecutive Fiver run when the estimated group size reaches `medium_shard`,
  merges and converts the groups in parallel, and performs at most one final
  Hazel merge.
- Each converted group removes its source Fivers only after publication. The
  final merge similarly removes source Hazels after its replacement is
  published. Consolidation reuses only a partial merge that exactly matches
  the final Hazel input and removes other recoveries.
- Final sanitization requires one Hazel spanning the first through last living
  sequence bounds. Timestamped progress is internal to
  `Bigwig::consolidate(...)` and is silent unless `verbose` is true.

## Current Posting And Compression Status

- `ZlibCompressor` keeps one lazily initialized deflater and inflater per
  thread and resets them between independent blobs. The compressor object
  remains stateless and safe for concurrent use, output remains compatible
  with fresh best-compression zlib streams, and capacity uses
  `compressBound(...)`.
- `SimplePosting` decodes compressed fields directly into sized vectors,
  reuses thread-local output buffers, and bulk-appends in the ordered merge
  case while preserving compact point/no-value representations.
- `SimplePostingFactory::posting_from_merge(...)` implements minimal-interval
  semantics: innermost intervals survive, and when `(p, q)` is identical the
  newest source value survives. Callers must supply posting sources from oldest
  to newest; Fiver and Hazel shard merges do so in sequence order.
- Hazel's posting-log merge advances through sequential source directories,
  reads source postings without populating the query cache, and writes merged
  compressed records once before final assembly.
- On the user's large `build.sh` run, these posting/Hazel-loop changes reduced
  foreground consolidation from 1,259,340 ms to 1,231,490 ms. Hazel merge fell
  from 661,538 ms to 639,682 ms, while Fiver-to-Hazel conversion remained about
  501 seconds.
- The completed parallel pipeline then reduced consolidation to 317,915 ms:
  29 Fiver groups merged and converted in 32,002 ms and 37 Hazels merged in
  260,583 ms. The full history is in `ai/consolidation.md`.

## Current Ranking Notes

- `Forager` retains forager construction and annotation behavior, while
  `TfIdfStats` consumes the metadata parser directly.
- `TfIdfStats::make(...)` owns its ranking-view stemmer/tokenizer through
  private base `Stats` state initialized by constructor.
- New foragers canonicalize omitted names to `tf-idf` and omitted tags to
  `none`, and their metadata is selected through `@`, `:type:`, `:name:`, and
  `:tag:`. An empty Stats recipe first checks the legacy `@tf-idf:` feature,
  then falls back to the new literal `none` tag.
- New forage metadata calls its processed interval query `contents`.
  `TfIdfStats` falls back through legacy `gcl` and then `container`; `id` has no
  default and is only required by consumers such as TREC output.
- User verified the current compatibility path against older `b.meadow` and
  `c.meadow` indexes with pre-current metadata field names; both remained
  usable.
- Meadowlark ranking uses forager metadata defaults (`stemmer=porter`,
  `tokenizer=ascii`) rather than Warren-global DNA stemmer settings.
- New Meadowlark creation no longer writes a Warren-global `container`
  parameter. `container` remains valid for non-Meadowlark/older Warren-style
  uses and for ranking-view metadata.
- Bigwig direct DNA activation honors `parameters:[ stemmer:"..." ]`.
- User reported removing `container` from `a.meadow/dna` still worked.
- User reported post-fix ranking: `MRR @10: 0.18975923272843034`,
  `QueriesRanked: 6980`.
- Batch TREC ranking now builds per-thread local Warren/Stats/Ranker views from
  an unstarted source Warren plus explicit statistics name/recipe; `Stats` no
  longer owns a clone operation.
- `apps/rank` accepts `--statistics`, `--stats`, and `-s` with `name[:recipe]`.
  It leaves Warren start/clone/statistics lifecycle to `trec(...)`.
- `trec(..., threads=0)` selects the internal thread cap. Explicit thread
  counts are still capped by `2 * hardware_concurrency()` and query count.
- `trec(..., addr *time = nullptr)` reports the maximum per-worker ranking-loop
  time in milliseconds when provided. On failure, the output time remains `0`.
  `apps/rank --verbose` reports this timing, not outer wall time around setup
  and result printing.
- The refactor removes the old serialized `Stats::clone()` setup bottleneck.
  With the started-Warren clone invariant, per-thread Warren clones are expected
  to bind to the same read snapshot when cloned from an already-started source
  Warren.
- SSR-derived ranking results use `p/q` for the best/shortest matching passage
  and `container_p/container_q` for the interval passed to SSR as its ranking
  container. Wrappers such as `tiered_ranking(...)` must preserve both ranges.
- In `apps/ssr-server`, the command-line `content` query is the rankable
  interval query passed to SSR, so `RankingResult::container_p/q` should be
  read as ranked content bounds. The command-line `container` query is the
  outer document/item unit used for full-document lookup and docno identity.
- `apps/ssr-server` emits qids as zero-based `qN` strings and normalizes JSON or
  TREC-style docno text before returning it. The `document` operation is
  stateless: for each request it runs
  `(>> {container} (>> {docno} "{requested-docno}"))` against every collection,
  errors on zero or multiple matching containers, and translates either the
  whole outer container or the optional comma-separated `--fields` intervals
  inside that container. Field-assembled documents preserve the caller's field
  order, strip outer quotes from each translated piece, and join pieces with
  ` ... `.

## Ranking Measurements

- Consolidated historical performance-shape notes live in `ai/hazel.md`.
- `apps/rank --verbose` reports the max per-worker ranking-loop time, not outer
  `trec(...)` wall time.
- On 2026-07-04, user reran `make testing`: `//test:hazel_test`,
  `//test:optimizer_test`, and `//test:tests` all passed.
- On 2026-07-04, user reran MARCO dev-small `rank.sh` checks with
  `bm25:b=0.68`, `bm25:k1=0.82`, `bm25:depth=10`, `stop`, `stem`, and `bm25`.
  `a.meadow/` reported `Ranking took: 12136 ms`, `0:13.73` wall,
  `5958648` KB max RSS, `MRR @10: 0.1897873743575748`, and
  `QueriesRanked: 6980`. `b.meadow/` reported `Ranking took: 6720 ms`,
  `1:23.37` wall, `21597860` KB max RSS, `MRR @10: 0.1896242666120888`, and
  `QueriesRanked: 6980`.
- Both MARCO runs emitted the known fake-result topics `645252` and `970152`.
  The small MRR difference was inspected via local `temp` diff output: most
  differences were same `(topic, docid)` rows with only rank changed, plus
  small top-10 boundary swaps. This is consistent with tie/order differences
  from highly parallel database build order, not a broad semantic change.
- On 2026-07-21, a newly built `a.meadow/` reported `Ranking took: 14863 ms`,
  `0:16.47` wall, `6165964` KB max RSS, MRR@10 of
  `0.18971858370855488`, and `QueriesRanked: 6980`. It emitted the same two
  known fake-result topics; the MRR remains within historical build-order
  variation.
- On 2026-07-26, the user reports that `make testing` passes after the
  posting-log recovery and consolidation changes. The user also repeatedly
  interrupted and resumed background merging with `fluffy`, ranking between
  activations, without observing a semantic failure.
- The final parallel-consolidation build `a.meadow/` reported
  `MRR @10: 0.18948731068358557`; an independently built `d.meadow/` reported
  `0.1895927707281574`. Each result was stable across repeated ranking runs.
  Both ranked 6,980 topics and emitted only the two established fake-result
  topics.
- Comparing the saved top tens found 883 changed topics: 724 retained the same
  top-10 set in a different order, 159 changed at the cutoff, 39 changed the
  first-ranked document, and 40 changed reciprocal rank. Those 40 changes
  exactly account for the MRR delta.
- Ranking sorts by score without a document-id tie-break, while `apps/rank`
  writes synthetic descending output scores rather than the underlying BM25
  values. The stable-per-index, different-between-index pattern and dominance
  of reorderings are consistent with equal-score traversal-order churn caused
  by different physical merge layouts, not a partially incorrect merge.

## Current Local Worktree Notes

- Do not rely on this section as durable truth without checking `git status`.
- No durable local worktree state is recorded here. Check `git status` after
  every restart.
