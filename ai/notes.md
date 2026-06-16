# Cottontail Repository Notes

## Top-Level Layout

- `src/`: core Cottontail library. Public umbrella header is
  `src/cottontail.h`.
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
- Query execution: `gcl.*`, `parse.*`, `hopper.*`, `array_hopper.*`,
  `vector_hopper.*`, `eval.*`, `ranking.*`, `ranker.*`.
- Text/feature processing: `tokenizer.*`, `ascii_tokenizer.*`,
  `utf8_tokenizer.*`, `featurizer.*`, `hashing_featurizer.*`,
  `json_featurizer.*`, `tagging_featurizer.*`, `vocab_featurizer.*`,
  `stemmer.*`, `porter.*`.
- Compression/stats/support: `compressor.*`, `post_compressor.*`,
  `tfdf_compressor.*`, `zlib_compressor.*`, `bad_compressor.h`, `stats.*`,
  `df_stats.*`, `idf_stats.*`, `field_stats.*`, `read_gate.h`.

## Meadowlark Map

- `meadowlark/meadowlark.*`: Meadowlark lifecycle and ingestion helpers.
- `create_meadow(...)` creates a Bigwig-based meadow with UTF-8 tokenizer, JSON
  featurizer, zlib text/fvalue compression, and post posting compression.
- `append_tsv(...)` and `append_jsonl(...)` ingest datasets, clone Warrens for
  parallel work, and annotate records with path/container metadata.
- `meadowlark/forager.*`: pluggable annotation passes over intervals or GCL
  query results.
- Current foragers include `tf-idf_forager.*` and `null_forager.h`.

## CLI Surfaces

- `apps/meadowlark.cc`: create/open a meadow and append `--tsv`/`--jsonl`
  inputs.
- `apps/forage.cc`: run a Meadowlark forager over a query; supports
  `--key value` and `--key=value` parameters.
- `apps/fluffy.cc`: interactive GCL query shell over a burrow or Hazel.
- `apps/rank.cc`: ranking CLI.
- `apps/simple.cc`: build a simple burrow from TREC/MARCO-style corpora.
- `apps/fiver2hazel.cc`: convert live Fiver shards in a burrow to Hazel shards
  with `--convert`, merge available Hazel shards with `--merge`, and time the
  conversion/merge phases. Existing exact per-Fiver Hazels are reused, and
  intermediate Hazels are preserved after merge. With no mode flags it does
  both phases.
- `apps/scratch.cc`: scratch utility for creating no-merge Bigwig/Fiver shards
  from small text files with `line:` and `file:` annotations.

## Build And Verification

- `MODULE.bazel` defines the Bazel module with `nlohmann_json`, `googletest`,
  and `rules_cc`.
- `src/BUILD` exports `//src:cottontail`, including `src/*` and the Meadowlark
  library.
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
- Hazel idx activation uses `OwslaCache`-backed waitable `SimplePosting` cache
  entries. Non-inline posting hoppers share cached postings; query-time cache
  fills use a 16-reader `ReadGate`, run in a background thread, and publish
  completion through `SimplePosting::release()`. Hazel's concrete
  `posting(feature)` method fills synchronously on the caller's thread and
  returns a ready posting.
- Hazel txt activation loads the text map, uses a 16-reader `ReadGate`, keeps a
  mutex-protected `text_chunk_tag` hopper, and caches decompressed chunks
  without eviction.
- `Hazel::merge(...)` merges compatible Hazel shards into a canonical
  `hazel.<start>.<end>` shard.
- Hazel merge sidecars now live beside the final shard as
  `mrg.hazel.<start>.<end>`, `pst.hazel.<start>.<end>`, and
  `dct.hazel.<start>.<end>`. The merge path still cleans old-style
  `hazel.<start>.<end>.*` sidecars as a transition aid.
- Hazel merge async read-ahead was tried and measured on HDD, but the gain was
  only about 1%; details and the recommendation not to carry that complexity
  forward are recorded in `ai/hazel-progress.md`.
- `test/hazel.cc` is the completed Hazel regression test. It builds a no-merge
  Bigwig from small text files, converts each Fiver to Hazel, compares
  Fiver-vs-Hazel shard behavior, merges Hazels, and compares the merged Hazel
  against the source Bigwig with null, real, and bad compressor profiles. It
  also checks that started Hazel clones stay started and remain readable after
  the source Hazel is ended.
- User ran `bazel test //test:hazel_test` successfully on 2026-05-26.
- Hazel performance milestones and rank.sh measurements live in
  `ai/hazel-progress.md`.

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
- Bigwig's sanitizer coordinator combines the inventories into the required
  `[ Hazel prefix ][ Fiver suffix ]` shape. The final Hazel may shadow leading
  Fivers at the boundary; those Fivers are deleted so the Hazel wins. Any other
  mixed Hazel/Fiver overlap or out-of-order Fiver-before-Hazel relationship is
  rejected. Hazel activation is the next wiring step.
- `BigwigIdx` is still Fiver-only. Its multi-Fiver path now handles empty and
  single-shard cases directly, merges `text_chunk_tag` postings fresh without
  caching, and uses the captured `OwslaCache` only for true normal
  multi-Fiver posting merges.
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
  `posting(feature)`. Fiver and Hazel both subclass `Owsla`.
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

## Current Ranking Measurements

- `rank.sh` measurements live in `ai/hazel-progress.md`. After 2026-06-07,
  `Ranking took:` from `apps/rank --verbose` is the max per-worker ranking-loop
  time, not outer `trec(...)` wall time.
- `a.meadow/` in the repo root currently contains three large Fivers
  (`0..11`, `12..37`, `38..58`) plus matching per-Fiver Hazels and merged Hazel
  `hazel.00000000000000000000.00000000000000000058`.
- `b.meadow/` currently contains one large Fiver covering `0..58`.
- User-reported 2026-06-07 MARCO dev small runs with requested `--threads
  2000`:
  - merged Hazel from `a.meadow`: max worker ranking loop `12583` ms, wall
    `0:14.08`, CPU `1523%`, max RSS `5659140` KB, `MRR @10:
    0.18975923272843034`.
  - `a.meadow`: max worker ranking loop `25891` ms, wall `1:46.29`, CPU
    `640%`, max RSS `67673128` KB, same `MRR @10`.
  - `b.meadow`: max worker ranking loop `6980` ms, wall `1:22.42`, CPU
    `301%`, max RSS `21804204` KB, `MRR @10: 0.1896242666120888`.
- These measurements support the Fiver/Hazel blend direction: large Fivers can
  be very fast once hot but expensive to open and hold in memory; merged Hazel
  has a slightly slower hot loop on this workload but much lower wall time and
  memory use.
- User-reported 2026-06-16 regression/ranking checks after the `OwslaCache`
  work passed. The three-Fiver `a.meadow` hot ranking loop improved from
  `25891` ms to about `9.1` seconds with unchanged MRR/query count, while the
  single-Fiver `b.meadow` path stayed stable at `6706` ms and unchanged
  MRR/query count.

## Active Mid-Flight Plan

- `ai/plan.md` may describe Hazel integration work that is intentionally in
  progress across multiple coding steps. Do not assume every item in the plan
  is unimplemented; compare the plan with `ai/log.md`, current code, and this
  notes file.
- The current Hazel/Bigwig integration direction is a conservative
  Fiver/Hazel blend. The next implementation step is narrower than full
  lifecycle integration: include already-existing Hazel shards in Bigwig read
  snapshots without physical merging, background conversion, or retention
  policy.
- Some prerequisites may already be complete while later steps remain gated for
  discussion or approval.
- Before implementing from `ai/plan.md`, honor any explicit discussion gate in
  the plan and confirm the intended next step with the user.

## Current Hazel/Bigwig Integration Status

- Hazel writer, standalone activation, Hazel-to-Hazel merge, conversion tools,
  and dedicated Hazel regression coverage are in place.
- Bigwig's started view still narrows live shards to `Fiver`. The next planned
  step is to make Bigwig query already-existing Hazel shards too, without
  merging Fiver and Hazel postings physically.
- Fiver and Hazel are now both `Owsla` subclasses with concrete
  `posting(feature)` accessors. The mixed Fiver/Hazel physical merge path
  remains separate from the no-merge visibility step.
- The no-merge Hazel visibility step should use public Warren APIs where
  possible. `BigwigTxt` already only needs public txt methods; `BigwigIdx`
  already owns posting composition/caching for the Fiver-only view and should
  extend that mergepoint to `Owsla` children for the mixed Hazel/Fiver view.
- Fluffle cache lifetime is no longer an open design question for the current
  code: the cache generation is shared across starts and invalidated when the
  visible logical snapshot changes through commit publication.
- Background lifecycle policy remains separate: creation timing, Fiver
  retention, activation preference, Fluffle mixed-range representation, and
  safe background publication still require discussion.
- See `ai/plan.md` for the concrete review checkpoint and `ai/hazel.md` for the
  Hazel format/activation/merge reference.

## Current Local Worktree Notes

- Do not rely on this section as durable truth without checking `git status`.
- No durable local worktree state is recorded here. Check `git status` after
  every restart.
