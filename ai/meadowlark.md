# Meadowlark Format And Append Plan

Status date: 2026-07-20.

This note records the current Meadowlark format, implemented ingestion
behavior, agreed metadata conventions, and the next planned append work. It
distinguishes current implementation from agreed but not-yet-implemented
design. No entry here authorizes coding without separate user confirmation.

## Mental Model

Meadowlark uses short punctuation features as a small, machine-facing schema.
The convention is intentionally compact because Meadowlark is increasingly a
library and database format for programmatic consumers and agents rather than
a query surface designed primarily for people.

The currently established punctuation features are:

- `/` marks source identity strings, currently normalized filenames.
- `:` marks ordinary input objects or records.
- `::` marks a TSV header record when `append_tsv(..., header = true)` is used.
- `@` marks JSON metadata records that describe annotations or source formats.

The normalized source identity is also featurized and used to annotate content
that came from that source. File metadata deliberately records the same identity
twice: as a `file` member in its JSON payload and as the feature of an annotation
containing the metadata interval. The JSON is self-describing, while the
annotation supports direct structural lookup.

When an agent connects to a Meadowlark database, its basic discovery operations
should include `/` and `@`:

- `/` discovers the sources represented in the meadow.
- `@` discovers metadata that explains derived annotations and source formats.

Metadata about a file must be contained in an annotation whose feature is the
normalized file identity. A query such as

```text
(<< @ /data/hdd3/Collections/msmarco/collection.tsv)
```

can then return only the metadata records describing that file. This association
is an agreed convention for future file metadata; TSV metadata does not yet
implement it.

## Metadata Records

An interval annotated with `@` contains a JSON object. The JSON is the
serialization of the metadata payload; it does not have to be ingested through
`json_scribe(...)` or receive the ordinary `:` object annotation. Keeping `@`
as its Meadowlark role prevents metadata from appearing in ordinary data-object
queries.

The top-level `type` member identifies the kind of annotations or source format
described by the record:

```json
{
  "type": "..."
}
```

The metadata object is an extensible, type-discriminated record:

- Type-specific keys define the remainder of the object.
- Additional keys are allowed.
- Readers should ignore keys they do not understand when they can still
  interpret the record safely.
- For compatibility with existing meadows, a missing `type` is exactly
  equivalent to `"type": "forager"`.

The generic `@` annotation supports discovery. A metadata type may also define
more specific annotation features for efficient direct lookup.

Metadata describing a file has an additional common contract:

- The JSON object contains a top-level `file` member.
- `file` contains the normalized file identity used by Meadowlark.
- The metadata interval annotated with `@` is contained in an annotation whose
  feature is that same normalized file identity.
- The JSON `file` value and the containing annotation feature must agree.

This redundancy is intentional. The JSON record remains meaningful when read
on its own, and containment makes file-specific metadata selection an index
operation rather than a scan through every `@` record.

### Forager Metadata

A forager metadata record describes one run of a derived annotation process.
Its core shape is:

```json
{
  "type": "forager",
  "name": "tf-idf",
  "tag": "passages",
  "parameters": {
    "start": "100",
    "end": "200",
    "gcl": ":passage:",
    "container": ":",
    "id": ":passage_id:",
    "stemmer": "porter",
    "tokenizer": "ascii"
  }
}
```

Forager metadata follows these conventions:

- `name` identifies the forager implementation and annotation family.
- `tag` identifies a particular output or statistics view within that family;
  it may be empty.
- `parameters` contains `start` and `end` for the processed address range.
- Other parameters depend on the forager.
- Other top-level keys may be added.

Existing forager records contain `name`, `tag`, and `parameters` but omit
`type`. They remain valid and must be interpreted as `type = "forager"`.
The existing parser already ignores unknown top-level keys, so future writers
can emit the explicit type without making the records unreadable by current
forager consumers.

The current TF-IDF forager annotates its JSON record with both `@` and a lookup
feature derived from its name and tag:

```text
@tf-idf:             # empty/default tag
@tf-idf:passages:    # tag = "passages"
```

The metadata record is a manifest for the run. The numeric results live under
related feature namespaces, including:

```text
tf-idf:passages:tf:<term>
tf-idf:passages:df:<term>
tf-idf:passages:total:items
tf-idf:passages:total:length
```

`TfIdfStats` uses the selected tag to find the corresponding metadata, validate
its `name` and `tag`, recover its queries/tokenizer/stemmer, and access the same
tagged statistics. BM25 is selected separately as a ranking method; the tag
selects which TF-IDF statistics view BM25 consumes.

Current details to regularize later:

- An empty forager name selects the TF-IDF implementation in some call paths,
  but the durable metadata name should be the canonical explicit `tf-idf`.
- Repeating a `(name, tag)` pair can create multiple manifests and accumulated
  statistics. Replacement, rejection, or another uniqueness policy has not yet
  been designed.

### TSV Metadata

The agreed TSV metadata type is `"tsv"`. It describes the interpretation of a
TSV source, including the mapping from source headers to emitted annotation
features. The intended shape is:

```json
{
  "type": "tsv",
  "file": "/data/hdd3/Collections/msmarco/collection.tsv",
  "separator": "\t",
  "header": true,
  "columns": [
    {
      "index": 0,
      "header": "passage id",
      "feature": ":passage_id:"
    },
    {
      "index": 1,
      "header": "passage",
      "feature": ":passage:"
    }
  ]
}
```

The column mapping is an array so it preserves source order, empty headings,
duplicate headings, and any normalization collisions. Each entry records the
original header and the feature actually emitted by ingestion.

When a header is present, its column annotations must not contain whitespace so
that they remain usable as raw GCL features. Historical Meadowlark behavior
replaced header whitespace with `_` and wrapped the result in colons, for
example:

```text
Favorite Food -> :Favorite_Food:
Animal        -> :Animal:
```

The current intended direction is to restore header-derived labels while
retaining numeric `:0:`, `:1:`, and similar labels when no header is declared.
Exact policies for empty headings, extra columns, duplicate normalized labels,
and GCL-significant non-whitespace characters should be settled before coding.

The TSV JSON should be appended as metadata text and annotated with `@`. Its
interval must be contained in an annotation for the exact normalized identity
stored in `file`. The metadata, source marker, and TSV data should be committed
atomically. This does not require a `Scribe`; TSV remains a direct
Warren/Appender/Annotator ingestion path. A possible additional type-specific
lookup annotation has not yet been chosen.

It is acceptable for this capability to exist first as a library API even when
no command-line application exposes the `header` argument.

## Source Identity And Restart

Restartability needs a durable identity string for each append operation.
File-based appends currently use a normalized filename. Non-file appends must
receive an explicit source label or append id; otherwise duplicate detection is
not well-defined.

The current identity representation has two parts:

1. The normalized source string is appended as text and annotated with `/`.
2. The featurized source string annotates the content written from that source.

`already_appended(...)` searches `(>> / "<source>")`, translates candidate
intervals, and checks the stored normalized source string. `apps/meadowlark.cc`
parses its inputs into typed records, starts the Warren once, preflights every
source, and then skips committed sources before dispatching missing inputs in
command-line order.

The central restart invariant is:

> Rerunning Meadowlark with the complete source list skips fully committed
> sources and safely retries missing or interrupted sources.

The source marker must therefore participate in the same coordinated
publication as the source content and source metadata. Publishing `/` before
the content is complete can cause restart to mistake a partial append for a
completed one.

Current path normalization is deliberately small: a filename containing no `/`
is prefixed with `./`; other strings are unchanged. Whether source identities
should eventually use stronger canonicalization remains open.

## Common Append Lifecycle

Append operations should share one lifecycle while writing through their
narrowest appropriate abstraction:

1. Establish or receive the durable source identity.
2. Start the original Warren at the top of the append operation.
3. Use that started Warren as the read snapshot and clone source.
4. Open a transaction for the source marker and any source metadata.
5. Create worker clones from the started Warren.
6. Write through `Scribe` only when the input adapter requires it; otherwise
   remain at the Warren/Appender/Annotator layer.
7. Ready the source transaction and every worker transaction.
8. If any setup, write, or ready step fails, abort every transaction and end
   every clone.
9. On success, publish all transactions through the highest applicable batch
   helper.
10. Finalize any Scribes used by the operation.
11. End every clone and then the original Warren on every return path.

If an append path writes through `Scribe`, it should commit through
`Scribe::commit_all(...)`. A direct Warren path should commit through
`Warren::commit_all(...)`. These helpers can discover coordinated Bigwig
publication; Meadowlark should not inspect or depend on the concrete Warrens or
their shared working directory.

Duplicate preflight may be performed by a batch caller such as the Meadowlark
CLI. The public append APIs and their eventual non-file variants still need a
deliberate contract for whether they also provide independent idempotence.

## Implemented Append State

### JSONL Files

`append_jsonl(warren, filename, ...)` is the current model for coordinated
append publication:

- It starts the original Warren and ends it through one top-level finish path.
- It streams plain or gzipped input with `maybe_zipped(...)`.
- It writes and readies the source marker through a Warren-backed `Scribe`.
- It creates workers by cloning the already-started Warren.
- It writes JSON records through per-clone Scribes.
- It annotates each nonempty JSON record with the source-identity feature.
- It readies all worker Scribes.
- It aborts the source and workers together on failure.
- It publishes the source marker and workers through `Scribe::commit_all(...)`.
- It finalizes the Scribes and ends every clone and the original Warren.

The CLI performs duplicate preflight before calling this function. The append
function itself does not currently repeat that check.

### TSV Files

`append_tsv(warren, filename, ...)` has not yet been moved to the coordinated
lifecycle:

- It reads the complete file and splits it into lines.
- It writes, readies, and commits the source marker before worker ingestion.
- It creates direct Warren transactions and explicitly starts each clone.
- It partitions lines into contiguous worker ranges.
- It annotates ordinary rows with `:` and an optional header row with `::`.
- It currently annotates fields with numeric features such as `:0:` and `:1:`.
- It commits worker clones individually.
- It does not emit `type = "tsv"` metadata.

The early source-marker commit violates the restart invariant: a failed or
interrupted TSV append can leave a durable `/` marker for incomplete data, and
a later CLI replay can then skip that source.

TSV does not need Scribe. The planned repair is to keep direct Warren
transactions, hold the source marker and TSV metadata transaction ready, ready
all worker Warrens, and publish the original Warren plus its worker clones with
`Warren::commit_all(...)`. Failure must abort all of them before ending the
started views.

## Planned Append Surfaces

### JSONL From Strings

Add an append operation for an array or vector of JSON strings. It should share
the JSON record worker body and Scribe lifecycle with file JSONL while taking an
explicit source identity from its caller.

The initial API can accept already-split strings unless a more general streaming
input abstraction becomes clearly useful.

### Raw Text Files

Add raw-text ingestion through the same source identity, metadata, transaction,
and restart conventions. The first version can treat each file as one record
and annotate the full content interval with its source-identity feature.

### Code

Code ingestion should be another source adapter rather than another commit
mechanism. Its initial form can append each source file as raw text and record
language, repository, and path metadata. Parser-aware symbols, definitions,
imports, and references can be later annotation passes once the storage
lifecycle is settled.

## Next Agreed Design Area

The next implementation area, after separate explicit coding approval, is
`append_tsv(...)`:

1. Preserve direct Warren/Appender/Annotator writing; do not introduce Scribe.
2. Move source marker and workers onto one coordinated start/ready/commit/abort
   lifecycle modeled on JSONL.
3. When a header is declared, use whitespace-free header-derived column
   features rather than only numeric features.
4. Emit one `type = "tsv"` JSON metadata record containing the normalized
   `file` identity and its column-to-feature mapping.
5. Annotate that metadata with `@` and contain it in an annotation for the same
   file identity so structural file-metadata queries work.
6. Keep typed input interpretation reusable for future library callers and the
   deferred static-shard builder.

Do not begin this work without explicit user confirmation.
