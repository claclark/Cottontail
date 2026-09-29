# Examples from the IRRJ Paper

Programs for the two experiments described in:

Charles L. A. Clarke. 2025. Annotative Indexing. *Information Retrieval Research*
1, 1 (2025), 109–136. https://doi.org/10.54195/irrj.19910

- **TREC collection building and updating:** [trec-example.cc](trec-example.cc),
  with [trec-example.py](trec-example.py) for summarizing the reported scores
  over time.
- **JSON tables:** [json-examples.cc](json-examples.cc), with
  [jsonl.cc](jsonl.cc) for building the JSON collection.

Build from the repository root with `bazel build //iirj:all`. Compiled programs
are placed in `bazel-bin/iirj/`.
