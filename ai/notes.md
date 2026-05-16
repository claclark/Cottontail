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
