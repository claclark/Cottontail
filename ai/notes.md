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
- `gcl/optimizer.*` currently provides an opt-in S-expression optimizer shell.
  Optimization defaults off; callers can use `Optimizer::enable()` for
  experiments and `Optimizer::disable()` to restore the baseline path.
- The first rewrite experiment reordered and nested
  `(<< (^ a b c ...) Q)` by estimated term count. On the user's old single
  SimpleWarren shard containing nearly 1 TB of raw text, this was a negative
  result: the longer rewritten/title-style `winnie the pooh` query was roughly
  one minute, while the two-term innermost probe `winnie pooh` with the same
  window shape was about `46` ms.
- The current materialization experiment adds optimizer-generated
  `(materialize X)`, which fully enumerates `X` once into an `ArrayHopper`
  before passing it outward. The contained-in/all-of rewrite now materializes
  each nested level. `Link` is the existing precedent for lazy child
  materialization into an array-backed hopper.
- `apps/ssr-server` serves JSON-line SSR requests over localhost for one or
  more burrows, and `apps/ssr-client` is the readline client for interactive
  query/next-result use. `apps/ssr-client.py` is the standard-library Python
  example client for the same protocol.
- `SimpleIdx` posting-cache eviction is currently compiled out with
  `COTTONTAIL_SIMPLE_IDX_CACHE_EJECTION` set to `0`; cached postings remain for
  the life of the `SimpleIdx` unless the idx is reset or destroyed.

## Meadowlark Map

- `meadowlark/meadowlark.*`: Meadowlark lifecycle and ingestion helpers.
- `create_meadow(...)` creates a Bigwig-based meadow with UTF-8 tokenizer, JSON
  featurizer, zlib text/fvalue compression, and post posting compression.
- `append_tsv(...)` and `append_jsonl(...)` ingest datasets, clone Warrens for
  parallel work, and annotate records with path/container metadata.
- `apps/meadowlark.cc` preflights typed input files in one started-Warren pass
  using `meadowlark::already_appended(...)`, then skips already-present files
  before dispatching appends.
- `meadowlark/forager.*`: pluggable annotation passes over intervals or GCL
  query results.
- Current foragers include `tf-idf_forager.*` and `null_forager.h`.

## CLI Surfaces

- `apps/meadowlark.cc`: create/open a meadow, preflight existing input files,
  and append missing `--tsv`/`--jsonl` inputs.
- `apps/forage.cc`: run a Meadowlark forager over a query; supports
  `--key value` and `--key=value` parameters.
- `apps/fluffy.cc`: interactive GCL query shell over a burrow or Hazel.
- `apps/rank.cc`: ranking CLI.
- `apps/ssr-server.cc`: localhost JSON-line SSR server over one or more
  burrows. It takes `container content docno burrow...`, binds an automatic
  localhost port, and reports `listening on port N` for clients. Query results
  rank within `content`; `container` is used to find docno/document identity.
  Snippets stay within the ranked content interval, highlight with
  `<cover>...</cover>`, clip oversized covers to the first 200 tokens, and keep
  up to 1000 covers available through `next`.
- `apps/ssr-client.cc`: readline client for `ssr-server`; non-empty input sends
  a query and empty input requests the next result.
- `apps/ssr-client.py`: no-dependency Python example client for the same
  JSON-line protocol.
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
- `apps/scratch.cc`: scratch utility for creating no-merge Bigwig/Fiver shards
  from small text files with `line:` and `file:` annotations.

## Build And Verification

- `MODULE.bazel` defines the Bazel module with `nlohmann_json`, `googletest`,
  and `rules_cc`.
- `src/BUILD` exports `//src:cottontail`, including `src/*`, `gcl/*`, and the
  Meadowlark library.
- `apps/BUILD` contains standalone `cc_binary` targets.
- `test/BUILD` contains aggregate `//test:tests` and dedicated
  `//test:hazel_test`.
- Repository rule: agents should run compile/build checks only. Do not run test
  cases, including `bazel test`, unless the user explicitly asks for that
  specific test run.

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
- `Hazel::merge(...)` merges compatible activated Hazel shards into a
  caller-supplied destination and returns an activated but unstarted output
  Hazel. Working/name adapters preserve the existing bool-returning command
  surfaces.
- Hazel merge sidecars now live beside the final shard as
  `mrg.hazel.<start>.<end>`, `pst.hazel.<start>.<end>`, and
  `dct.hazel.<start>.<end>`. The merge path still cleans old-style
  `hazel.<start>.<end>.*` sidecars as a transition aid.
- Hazel merge async read-ahead was tried and measured on HDD, but the gain was
  only about 1%; the recommendation is not to carry that complexity forward
  without stronger evidence of an I/O bottleneck.
- `test/hazel.cc` is the completed Hazel regression test. It builds a no-merge
  Bigwig from small text files, converts each Fiver to Hazel, compares
  Fiver-vs-Hazel shard behavior, merges Hazels, and compares the merged Hazel
  against the source Bigwig with null, real, and bad compressor profiles. It
  also checks that started Hazel clones stay started and remain readable after
  the source Hazel is ended.
- User ran `bazel test //test:hazel_test` successfully on 2026-05-26.
- Consolidated Hazel format, activation, merge, Bigwig integration, and
  performance-shape notes live in `ai/hazel.md`.

## Current Bigwig Cache Status

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
- Static indexes reuse the same cache generation across `end()` -> `start()`
  cycles until a visible commit changes the logical snapshot.
- Constructing a `Working` object now removes generic `temp.*` files, covering
  both new and existing burrows.
- Bigwig directory startup now runs `sanitize(...)` in `src/bigwig.cc`.
  Communication records `OwslaShard` and `HazelMergeRecovery` live in
  `src/owsla.h`. `Fiver::sanitize(...)` owns strict Fiver parsing,
  same-type contained-shard cleanup, and `kitten*` removal.
  `Hazel::sanitize(...)` owns strict Hazel parsing, same-type contained-shard
  cleanup, old sidecar cleanup, private merge sidecar cleanup, and returning
  logical restartable Hazel merge records.
- Bigwig's sanitizer coordinator allows mixed Hazel/Fiver order. Fivers covered
  by Hazels are deleted so the Hazel wins; Fivers containing Hazels and partial
  mixed overlaps are rejected.
- Bigwig startup activates sanitized shards in sequence order, adds them to the
  Fluffle visible list, and captures started read snapshots as
  `std::vector<std::shared_ptr<Owsla>>`.
- Fluffle owns the sanitized pending Hazel merge recovery list as
  `hazel_merges`; startup copies it from `SanitizedInventory`. Hazel merge
  selection reads this list without mutating it, and merge-worker validation
  clears matching or conflicting recovery records when a Hazel action is
  accepted.
- Bigwig merge selection no longer uses a split/barrier. `find_merge_action(...)`
  asks `find_fiver_action(...)` first, then falls back to `find_hazel_action(...)`.
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

## Current Meadowlark/Ranking Notes

- `TfIdfStats::make(...)` owns its ranking-view stemmer/tokenizer through
  private base `Stats` state initialized by constructor.
- Meadowlark ranking should use `@tf-idf:` metadata/defaults
  (`stemmer=porter`, `tokenizer=ascii`) rather than Warren-global DNA stemmer
  settings.
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
  and `container_p/container_q` for the ranked container. Wrappers such as
  `tiered_ranking(...)` must preserve both ranges.
- In `apps/ssr-server`, the command-line `content` query is the ranked
  container passed to SSR, so `RankingResult::container_p/q` should be read as
  the ranked content bounds. The command-line `container` query is only the
  outer unit used to recover the normalized docno.
- `apps/ssr-server` emits qids as zero-based `qN` strings and normalizes JSON or
  TREC-style docno text before returning it. The `document` operation only
  accepts docnos previously emitted by this server session and translates the
  cached ranked content interval instead of scanning the whole docno list.

## Ranking Measurements

- Consolidated historical performance-shape notes live in `ai/hazel.md`.
- `apps/rank --verbose` reports the max per-worker ranking-loop time, not outer
  `trec(...)` wall time.

## Current Local Worktree Notes

- Do not rely on this section as durable truth without checking `git status`.
- No durable local worktree state is recorded here. Check `git status` after
  every restart.
