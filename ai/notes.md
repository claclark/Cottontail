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
- Hazel idx activation uses a CacheRecord-backed decoded posting cache.
  Non-inline posting hoppers share cache lines; cache fills use a 16-reader
  `ReadGate`.
- Hazel txt activation loads the text map, uses a 16-reader `ReadGate`, keeps a
  mutex-protected `text_chunk_tag` hopper, and caches decompressed chunks
  without eviction.
- `Hazel::merge(...)` merges compatible Hazel shards into a canonical
  `hazel.<start>.<end>` shard.
- Hazel merge async read-ahead was tried and measured on HDD, but the gain was
  only about 1%; details and the recommendation not to carry that complexity
  forward are recorded in `ai/hazel-progress.md`.
- `test/hazel.cc` is the completed Hazel regression test. It builds a no-merge
  Bigwig from small text files, converts each Fiver to Hazel, compares
  Fiver-vs-Hazel shard behavior, merges Hazels, and compares the merged Hazel
  against the source Bigwig with null, real, and bad compressor profiles.
- User ran `bazel test //test:hazel_test` successfully on 2026-05-26.
- Hazel performance milestones and rank.sh measurements live in
  `ai/hazel-progress.md`.

## Current Bigwig Cache Status

- A started `BigwigIdx` owns a `SafeMap<feature, SimplePosting>` cache for
  posting lists merged across its Fiver view. `Fiver::merge(...)` consults this
  caller-supplied cache.
- The cache is currently discarded with the started `BigwigIdx`; Fluffle does
  not own an idx cache.
- When deletions exist, `BigwigIdx` caches raw feature and `null_feature`
  merges separately and composes their hoppers with `NotContainedIn`.

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
- The refactor removes the old serialized `Stats::clone()` setup bottleneck,
  but it does not yet guarantee all per-thread Warren clones bind to the same
  read epoch. Clone-under-start or an explicit read snapshot remains a separate
  system-level design question.

## Current Plan

- Next Hazel milestone: review the concrete mixed Fiver/Hazel Bigwig query-path
  plan with the user before coding.
- The planned idx direction keeps Hazel's shard-local cache, adds asynchronous
  Bigwig raw posting-list merge caching when multiple sub-indexes contribute,
  delegates one-source features directly to the source idx hopper, and retains
  the existing `NotContainedIn(feature, null_feature)` deletion composition.
- A separate cache-lifetime question remains open: whether a Bigwig
  merged-posting cache should survive `end()` -> `start()` while the logical
  sequence remains unchanged. This is intentionally simpler than maintaining
  feature-by-sequence cache versions in Fluffle.
- Background lifecycle policy remains separate: creation timing, Fiver
  retention, activation preference, Fluffle mixed-range representation, and
  safe background publication still require discussion.
- See `ai/plan.md` for the concrete review checkpoint and `ai/hazel.md` for the
  Hazel format/activation/merge reference.

## Current Local Worktree Notes

- Current uncommitted work is the ranking/TREC lifecycle refactor in
  `src/stats.*`, `src/ranker.*`, and `apps/rank.cc`, plus matching `ai/`
  documentation updates.
- Agent verification for this work: `bazel build //apps:rank`,
  `bazel build //...`, and `git diff --check`.
- The user reported that the three ranking checks behaved as expected and that
  regression tests passed.
