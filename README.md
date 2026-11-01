# Cottontail

Cottontail aims to provide a unified storage substrate for agentic search.
Cottontail is built around annotative indexing, a unified approach to indexing
that currently supports inverted indices, links, graph structures, JSON, other
structured text, and dynamic indices. Transactions support immediate
updates with ACID properties for multiple concurrent readers and writers.
Planned support includes indexed regular expression search and dense retrieval;
you can see these and other plans developing in the [`ai/`](ai/) directory.

This repository also includes an initial version of a metadata layer for
Cottontail, called Meadowlark. Meadowlark provides a simple filesystem-like
layer over Cottontail, directly supporting JSONL and TSV files with structural
annotations, as well as content-oriented support for flat text files and code.
Meadowlark's metadata helps agents explore and understand previously unseen
databases, with the long-term goal of establishing a universal standard for
agentic stores.

Cottontail is the reference implementation of annotative indexing described in:

Charles L. A. Clarke. 2025. Annotative Indexing. *Information Retrieval Research* 1, 1 (2025), 109–136. https://doi.org/10.54195/irrj.19910

Current command-line utilities live in [`apps/`](apps/). Historical experiments
and benchmarks live in [`expr/`](expr/). The TREC collection-update and JSON-table
experiments from the paper, with their supporting programs, live in
[`iirj/`](iirj/). Shared library helpers live in `src/`.

> **Coding agents:** Read [AGENTS.md](AGENTS.md) before working in this
> repository.

## Demos

- [MS MARCO demo](msm-demo.md): build a Meadowlark collection, explore its
  structure and metadata, add ranking annotations, and run BM25 retrieval.
- [SSR agentic search demo](ssr-demo/README.md): watch the video, build and serve
  the 1.6 TB ClimbMix collection, and explore Boolean search through the web
  interface, command-line clients, or an agent.

## Installation

On Ubuntu or Debian, install a C++ compiler, Git, `curl`, and the zlib headers:

```sh
sudo apt update
sudo apt install build-essential git curl zlib1g-dev
```

Cottontail is built with [Bazel](https://bazel.build/). The easiest way to
install Bazel is to install
[Bazelisk](https://github.com/bazelbuild/bazelisk) under the name `bazel`:

```sh
sudo curl -L \
  https://github.com/bazelbuild/bazelisk/releases/latest/download/bazelisk-linux-amd64 \
  -o /usr/local/bin/bazel
sudo chmod +x /usr/local/bin/bazel
```

The slightly magical part is the filename: `/usr/local/bin/bazel` is actually
Bazelisk, a small launcher that downloads and runs Bazel. The first build will
therefore take longer and requires an internet connection. On an ARM64 Linux
system, use `bazelisk-linux-arm64` instead of `bazelisk-linux-amd64`.

Now clone and build Cottontail:

```sh
git clone https://github.com/claclark/Cottontail.git
cd Cottontail
make building
```

The compiled command-line tools will be in `bazel-bin/apps/`. Use `make fast`
instead when you want an optimized build.
