# Cottontail repository map

## Top-level layout

- `src/`: core Cottontail library. Public umbrella header is `src/cottontail.h`.
- `meadowlark/`: Meadowlark-specific layer built on top of the core library.
- `apps/`: CLI binaries and dataset/task-specific utilities.
- `test/`: single Bazel `cc_test` target with coverage across core and Meadowlark components.
- `*.burrow`, `*.meadow`: persisted local working directories / example indexes checked into the repo.

## Core architecture in code

- `src/warren.h`, `src/warren.cc`: central orchestration interface. Owns `Working`, `Featurizer`, `Tokenizer`, `Idx`, `Txt`, `Annotator`, and `Appender`. Query processing requires `start()` / `end()`.
- `src/simple_warren.h`, `src/simple_warren.cc`: flat-file Warren implementation. Reconstructs components from DNA metadata stored in a burrow.
- `src/bigwig.h` (+ implementation in `src/bigwig.cc`): higher-level Warren implementation with `Fiver` / `Fluffle` backing and merge support. Meadowlark creation goes through Bigwig.
- `src/working.h`: filesystem abstraction for a working directory. Responsible for naming, temp files, readers/writers, and optional preload hints.
- `src/simple.h`: canonical filenames and record layouts for the simple storage stack (`raw`, `map`, `txt`, `idx`, `pst`).

## Main component families under `src/`

- Storage / indexing: `idx.*`, `simple_idx.*`, `simple_posting.*`, `txt.*`, `simple_txt*`, `fastid_txt.*`
- Ingestion / mutation: `builder.*`, `simple_builder.*`, `appender.*`, `annotator.*`, `scribe.*`
- Query execution: `gcl.*`, `parse.*`, `hopper.*`, `array_hopper.*`, `vector_hopper.*`, `eval.*`, `ranking.*`, `ranker.*`
- Text processing: `tokenizer.*`, `ascii_tokenizer.*`, `utf8_tokenizer.*`, `featurizer.*`, `hashing_featurizer.*`, `json_featurizer.*`, `tagging_featurizer.*`, `vocab_featurizer.*`, `stemmer.*`, `porter.*`
- Compression / stats / support: `compressor.*`, `post_compressor.*`, `tfdf_compressor.*`, `zlib_compressor.*`, `stats.*`, `df_stats.*`, `idf_stats.*`, `field_stats.*`

## Meadowlark map

- `meadowlark/meadowlark.h`, `meadowlark/meadowlark.cc`: Meadowlark lifecycle and ingestion helpers.
- `create_meadow(...)` creates a Bigwig-based meadow with UTF-8 tokenizer, JSON featurizer, zlib text/fvalue compression, and post posting compression.
- `append_tsv(...)` and `append_jsonl(...)` ingest datasets, clone the Warren for parallel work, and annotate records with path/container metadata.
- `meadowlark/forager.h`, `meadowlark/forager.cc`: pluggable annotation passes over intervals or GCL query results.
- Current forager implementations present here: `tf-idf_forager.*`, `null_forager.h`.

## CLI surfaces worth remembering

- `apps/meadowlark.cc`: create/open a meadow and append `--tsv` / `--jsonl` inputs.
- `apps/forage.cc`: run a Meadowlark forager over a query; supports `--key value` or `--key=value` parameters.
- `apps/fluffy.cc`: interactive GCL query shell over a burrow.
- `apps/simple.cc`: build a simple burrow from TREC / MARCO-style corpora.
- `apps/containers.cc`, `apps/rank.cc`, `apps/tf-idf.cc`, `apps/jsonl.cc`, `apps/qap.cc`: additional task-specific utilities.

## Build / test

- `MODULE.bazel`: Bazel module with `nlohmann_json`, `googletest`, and `rules_cc`.
- `src/BUILD`: exports one `cc_library` named `cottontail` including `src/*` plus `meadowlark/*`.
- `apps/BUILD`: many standalone `cc_binary` targets; `meadowlark`, `forage`, `fluffy`, and `simple` are the most useful entry points for exploration.
- `test/BUILD`: one aggregate `cc_test(name = "tests")` over all `test/*.cc`.

## Operational notes

- Public include surface for most code is `#include "src/cottontail.h"`.
- Warren instances are usually opened with `Warren::make(...)`, then explicitly `start()`ed before accessing `idx()` / `txt()` and `end()`ed afterwards.
- Meadowlark validation is parameter-based: `format=meadowlark`.
- Use the Makefile targets for routine builds/tests; they keep Bazel command
  lines consistent (`make building`, `make debugging`, `make testing`,
  `make fast`).
- Featurizer persistent identity is object-owned: `Featurizer::name()` is
  virtual, `""` remains only an input alias for hashing in `Featurizer::make`,
  JSON/tagging wrappers report their wrapper names, and `vocab` passes through
  the wrapped featurizer identity because it is a build-time vocabulary tap.

## Current design direction

- Core indexing model: content stays primary; TSV, JSON, text, ids, containers, tf values, and ranking metadata are all expressed as annotations layered on top.
- GCL / the s-expression query language is a hopper-composition language: parse expressions, compose `Hopper` objects, and return new hoppers.
- `Warren` is the low-level engine and should remain accessible from Python for advanced use.
- `Meadow` is the Python-facing layer over a Warren. It should add metadata, conventions, and discoverability rather than replace the underlying engine.
- Practical Meadow metadata goals: list loaded files, list available indices / annotation families / ranking views, and resolve `id` / `container` / `content` definitions for ranking.
- `Stats` is an internal query-type / ranking-view adapter, not the primary Python abstraction. It currently works best for BM25-style ranking and does not yet cover all rankers cleanly.
- Current MVP target for Python: open/create a meadow, bulk ingest TSV or JSONL, materialize tf-idf annotations, and run a simple BM25-first ranking pipeline over one query or a batch of queries.
- Desired Python surface is roughly `results = meadow.rank(pipeline, queries)`, with multithreading handled internally in C++.
- The current standalone ranking work is intended to move app logic such as `rank.cc` into reusable library code that both CLIs and Python bindings can call.
- Long-term direction: move metadata/parameters out of Warren-era scaffolding and into persistent database-backed metadata, while evolving interfaces incrementally instead of doing large refactors.
- SPLADE support predates full JSON support; newer direction is to ingest model output as JSON-backed content/annotations and update ranking support around that.
- This is research programming: keep interfaces clean where possible, but prefer evolutionary progress and flexibility over premature abstraction freeze.

## Current Hazel status

- Hazel v1 writer and first activation pass are in place: standalone Hazel files can be recognized, parse idx/txt blobs, construct hoppers from posting blobs, translate text through cached decompressed txt chunks, and shallow-clone the Hazel Warren.
- `fiver2hazel` is currently a pragmatic conversion path from a Fiver pickle plus its enclosing burrow DNA; it now preserves the owner Warren `parameters` block in generated Hazel DNA.
- Hazel idx activation now has the first CacheRecord-backed decoded posting cache: non-inline posting hoppers share cache lines, async fills use a 16-reader `ReadGate`, and `HazelIdx::cache(feature)` fills synchronously when it creates the line.
- Hazel txt activation loads the txt map, uses a 16-reader `ReadGate`, keeps a persistent mutex-protected `text_chunk_tag` hopper, and caches decompressed text chunks without eviction.
- Ranking transition note: `TfIdfStats::make(...)` owns its ranking-view stemmer/tokenizer through `Stats`; the old Warren-global `stemmer` write has been disabled as a transitional artifact. Continue tracking parameter/metadata scoping in `ai/plan.md`.
- Current HazelIdx cache implementation has a clean `bazel build //apps:working`; rank.sh checks remain correct with `MRR @10: 0.18923028380406587` and `QueriesRanked: 6980`.
- Latest Hazel rank.sh measurements are about 12 seconds internal for 6,980 queries, or roughly 1.7 ms/query: HazelIdx 16-reader gate alone measured `11850` ms internally, while the noisy 16/16 HazelIdx/HazelTxt run measured `12111` ms.
- Record Hazel performance milestones and before/after measurements in `ai/hazel-progress.md`.
- A first `Hazel::merge(...)` implementation exists for Working-based Hazel compaction, using input/output merge state structs and one open stream per input Hazel; it has only been compile-checked. User wants to guide behavioral/regression testing.

## Restart Notes

- Do not change files unless explicitly asked; recent conversation included a
  diff/design review mode.
- Current uncommitted code changes include `src/hazel.h`, `src/hazel.cc`, and
  the small `src/fiver.cc` trailing separator prerequisite.
- Compile verification already run for Hazel merge: `bazel build
  //apps:fiver2hazel //apps:working` and `bazel build //...`.
- Open investigation: Meadowlark ranking with pipeline `bm25:b=0.68
  bm25:k1=0.82 bm25:depth=10 stop stem bm25` only works when
  `stemmer:"porter"` is hacked back into meadow DNA, even though Meadowlark is
  supposed to source the stemmer from `@` tf-idf metadata. The user confirmed
  removing the DNA stemmer reproduces the failure. No fix has been made.
