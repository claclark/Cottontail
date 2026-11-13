# Examples from the IRRJ Paper

Programs for the two experiments described in:

Charles L. A. Clarke. 2025. Annotative Indexing. *Information Retrieval Research*
1, 1 (2025), 109–136. https://doi.org/10.54195/irrj.19910

- **TREC collection building and updating:** [trec-example.cc](trec-example.cc),
  with [trec-example.py](trec-example.py) for summarizing the reported scores
  over time.

  - [trec-2026-11-13.png](trec-2026-11-13.png) updates Figure 7 with performance
    as of November 13, 2026. Although the system now supports on-disk dynamic
    collections and the run includes full compression, other system improvements
    have reduced overall elapsed time by approximately 35%, from about 2,150 to
    1,405 seconds.  [trec-2026-11-13.log](trec-2026-11-13.log) contains the data
    for the plot.

- **JSON tables:** [json-examples.cc](json-examples.cc), with
  [jsonl.cc](jsonl.cc) for building the JSON collection.

Build from the repository root with `bazel build //iirj:all`. Compiled programs
are placed in `bazel-bin/iirj/`.

`trec-example [--compress|--nocompress] [collection queries qrels]` enables
compression by default: `post` for postings and `zlib` for values and text.
Use `--nocompress` to use `null` compressors for all three. 
The output index is `trec.burrow` in the current directory.
