# Annotative Indexing

> **Coding agents:** Read [AGENTS.md](AGENTS.md) before working in this
> repository.

A unified approach to indexing that currently supports inverted indices, links,
graph structures, JSON, other structured text, dynamic indices, and transactions,
with planned support for regular expressions and dense retrieval.
You can see some of these plans developing in the `ai/` directory.

Charles L. A. Clarke. 2025. Annotative Indexing. *Information Retrieval Research* 1, 1 (2025), 109–136. https://doi.org/10.54195/irrj.19910

This repo contains a reference implementation for annotative indexing,
called Cottontail.
In and of itself, Cottontail is not a search engine or database,
but rather a unique approach to indexing that can form the foundation for
these systems.

This repo also includes an initial version of a metadata layer for Cottontail,
called Meadowlark.
Meadowlark provides a simple filesystem-like layer over Cottontail,
directly supporting JSONL and TSV files with structural annotations,
as well as providing content-oriented support for flat text files and code.

## A Small Demo

Installation instructions appear below.

We base our demo on the MS MARCO V1 collection,
which can be downloaded from the official
[MS MARCO passage-ranking dataset page](https://microsoft.github.io/msmarco/Datasets.html#passage-ranking-dataset).
Extracting `collection.tar.gz` produces a single 2.9 GB TSV file,
`collection.tsv`.
For this demo you will need:

- `collection.tsv`, the 8,841,823-passage collection;
- [`topics.msmarco-passage.dev-subset.txt`](https://raw.githubusercontent.com/castorini/anserini/master/src/main/resources/topics-and-qrels/topics.msmarco-passage.dev-subset.txt),
  Castorini's copy of the 6,980-query dev subset;
- [`qrels.msmarco-passage.dev-subset.txt`](https://raw.githubusercontent.com/castorini/anserini/master/src/main/resources/topics-and-qrels/qrels.msmarco-passage.dev-subset.txt),
  containing 7,437 relevance judgments for those queries; and
- Castorini's
  [`msmarco_passage_eval.py`](https://raw.githubusercontent.com/castorini/anserini/master/tools/scripts/msmarco/msmarco_passage_eval.py)
  evaluation script.

The demo assumes you are in the root directory of the repo with all four files
in the same directory.

We start by creating a Meadowlark index and adding the collection:

```sh
./bazel-bin/apps/meadowlark --create --tsv ./collection.tsv
```

The `--tsv` flag instructs Meadowlark to treat the file as TSV,
adding structural information allowing Cottontail to directly access
rows and columns.
On the 2017-era 14-core/28-thread Intel Xeon server used to develop this demo,
it takes about a minute to add the collection.
Cottontail tries to fully exploit hardware threads for parallelism.
If you run `htop`, or a similar program, during index creation,
you should see up to 100% utilization.

This command creates a directory with the default name `a.meadow`.
If you look in the directory you will see a file named `dna`,
which provides a manifest of the Cottontail components the meadow uses.
You will also see a bunch of files prefixed with `fiver`.
These are fragments created by individual threads during ingestion.

Let's have a look at the metadata by running an inspection utility:

```sh
./bazel-bin/apps/inspect --burrow a.meadow
```

It will take 20–30 seconds to activate the index because Fiver is a
memory-resident format,
used for fast building and immediate update,
and we just added a large file as a single transaction.
Once the index is active, you will see a `>>` prompt. In the examples below,
`>>` is the inspector prompt, not part of the query.
As we are inspecting the index,
in the background Cottontail is actively consolidating the index,
slowly moving towards a single disk-based fragment.

The query `/` lists the files in the index:

```text
>> /
(0,3): "./collection.tsv"
```

The numbers at the start are the addresses for the filename itself,
which is itself stored in the annotative index as data.
The filename can be used in structural queries to represent the contents
of the file. The query:

```text
>> (<< : ./collection.tsv)
```

specifies all data objects (`:`) contained in the file.
Since this is a TSV file, each row is a separate data object,
and the inspector will print the first 24.
The query:

```text
>> (>> (<< : ./collection.tsv) "University of Waterloo")
```

specifies data objects containing the phrase.
The query:

```text
>> (<< :0: (>> (<< : ./collection.tsv) "University of Waterloo"))
```

specifies the document IDs (column 0) of those objects.
You can exit from the inspector by pressing `Ctrl-D`.

Assuming that you have been inspecting the index for several minutes,
the directory `a.meadow` will now contain two files:
the `dna` and a single file with the prefix `hazel`, which uses the disk-based
Hazel format.
If you run the inspector again, it will activate in a second or two.

Let's add TF-IDF annotations for BM25.
Cottontail provides no special indexing support for BM25,
term and document weights are added as standard annotations,
which are interpreted by the ranker as ranking statistics.

```sh
./bazel-bin/apps/forage "--id=:0:" "--container=:" "--stemmer=porter" :1: tf-idf
```

The various parameters indicate how to interpret fields in data objects.
Adding the annotations may take a few minutes.
If you look in `a.meadow` after annotation, you will see lots of files
representing the current status of index consolidation after annotation.

We could run the ranker now, but let's finish the consolidation to give us a
single fragment, which will be faster for ranking.

```sh
./bazel-bin/apps/consolidate --verbose a.meadow
```

The verbose flag will give us progress reports on the consolidation.

```text
Bigwig::consolidate [1787673033272 ms]: Opening and sanitizing a.meadow...
Bigwig::consolidate [1787673033395 ms]: Opening and sanitizing took 122 ms
Bigwig::consolidate [1787673033395 ms]: Loading 30 Fiver(s)...
Bigwig::consolidate [1787673057528 ms]: Loading Fivers took 24132 ms
Bigwig::consolidate [1787673057528 ms]: Merging and converting 30 Fiver group(s) with 30 worker(s)...
Bigwig::consolidate [1787673089962 ms]: Fiver merging and conversion took 32433 ms; removed 30 source Fiver(s)
Bigwig::consolidate [1787673090246 ms]: Discarded 1 unused partial Hazel merge(s)
Bigwig::consolidate [1787673090246 ms]: Merging 32 Hazel(s)...
Bigwig::consolidate [1787673354482 ms]: Hazel merge took 264235 ms
Bigwig::consolidate [1787673355283 ms]: Removed 32 source Hazel(s)
Bigwig::consolidate [1787673355283 ms]: Sanitizing consolidated shards...
Bigwig::consolidate [1787673355315 ms]: Final sanitization took 31 ms
Bigwig::consolidate [1787673355315 ms]: Consolidation took 322042 ms
```

If you look in `a.meadow`,
you will again see the `dna` plus a single `hazel` about 3.4 GB in size.

Before ranking, let's inspect the metadata contained in the index,
which shows both the files and annotations.

```sh
./bazel-bin/apps/inspect --burrow a.meadow
```

At the inspector prompt, the query `@` shows the metadata records:

```text
>> @
(4,78): { "columns": [ { "feature": ":0:" , "index": 0.000000 } , { "feature": ":1:" , "index": 1.000000 } ] , "filename": "./collection.tsv" , "header": false , "separator": "\t" , "type": "tsv" }
(523259378,523259440): { "name": "tf-idf" , "parameters": { "container": ":" , "id": ":0:" , "stemmer": "porter" } , "query": ":1:" , "tag": "none" , "type": "forager" }
(523259441,523259475): { "filename": "./collection.tsv" , "name": "tf-idf" , "tag": "none" , "type": "forager" }
```

The records describe the TSV structure, define the TF-IDF annotations, and
record that those annotations have been added to `./collection.tsv`.
Exit the inspector with `Ctrl-D` before continuing.

You can continue to add files and annotations,
but the Hazel file itself can be treated as a standalone static shard.
In the command below we use `a.meadow` as the index (`--burrow`), but we could
instead specify the consolidated Hazel file directly.
Let's do some ranking with the standard MS MARCO "dev small" set of 6,980
queries:

```sh
./bazel-bin/apps/rank --verbose --burrow a.meadow topics.msmarco-passage.dev-subset.txt bm25:b=0.68 bm25:k1=0.82 bm25:depth=10 stop stem bm25 > temp.rank
```

The parameters specify the ranking pipeline.
The result will be a file in TREC format. The MS MARCO evaluation script
expects query ID, passage ID, and rank as three tab-separated columns. This
minimal `awk` command performs the conversion, using passage ID `0` for any
query with no results:

```sh
awk 'BEGIN { OFS="\t" } { print $1, ($3 == "FAKE" ? 0 : $3), $4 }' \
  temp.rank > temp.msmarco.rank
python3 msmarco_passage_eval.py qrels.msmarco-passage.dev-subset.txt temp.msmarco.rank
```

You should get output similar to:

```text
#####################
MRR @10: 0.18986020148269417
QueriesRanked: 6980
#####################
```

The exact MRR depends partly on the order of data objects in the index,
which in turn depends on which thread added which data object,
but it should fall in the range [0.189, 0.190].

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
