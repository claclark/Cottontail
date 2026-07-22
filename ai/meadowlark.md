# Meadowlark Format, Metadata, And Append Conventions

Status date: 2026-07-22.

This note records the current Meadowlark format, its machine-discovery and
metadata conventions, implemented ingestion behavior, and possible future
append surfaces. It distinguishes current implementation from agreed but
not-yet-implemented design. No entry here authorizes coding without separate
user confirmation.

## Self-Describing Format

Meadowlark uses short punctuation features as a small, machine-facing schema.
The convention is intentionally compact because Meadowlark is increasingly a
library and database format for programmatic consumers and agents rather than
a query surface designed primarily for people.

### Root Feature Roles

The established root features are:

- `/` marks the canonical source identity string, currently a normalized
  filename. File appenders write it once per source.
- `//` marks a segment-local copy of the source identity string.
- `/.` marks a file or file-segment envelope containing a `//` source identity
  followed by the metadata or data belonging to that segment.
- `:` marks ordinary input objects or records.
- `::` marks a TSV header record when `append_tsv(..., header = true)` is used.
- `@` marks JSON metadata records that describe annotations or source formats.

The `:` and `@` root roles are deliberately disjoint: metadata is not an
ordinary data object. A metadata object's members still receive ordinary
colon-path JSON field annotations such as `:type:` and `:file:`. Those field
annotations describe structure inside the `@` interval; they do not give the
metadata object a `:` root.

### Source Identity

File ingestion connects several representations of a source:

1. The source/metadata transaction begins with the normalized source string.
   That canonical interval receives both `/` and `//`.
2. Every worker transaction that consumes at least one input record begins
   with another copy of the normalized source string, annotated only with `//`.
3. A `/.` annotation wraps each such source string together with the metadata
   or data written in that transaction. Empty worker transactions emit neither
   a source-string copy nor an empty segment.
4. The normalized source string is itself featurized and annotates the data
   intervals written from that source.
5. Each source-format metadata record contains the same identity in its
   top-level `file` member.

The source-identity feature applies to file contents, including a TSV header
when present, but not to metadata or the segment-local filename. The `/` marker
is the durable, once-per-source identity used for append preflight. The `//`
and `/.` pair supports inverse provenance lookup from an interval to its source.
The `file` field makes metadata-to-source association explicit inside JSON, and
the source-identity feature selects the actual data.

### Agent Bootstrap

When an agent connects to a Meadowlark database, its basic discovery operations
on the current read snapshot are:

- `/` discovers the sources represented in the meadow.
- `@` discovers metadata that explains derived annotations and source formats.
- `(<< :type: @)` discovers the explicit metadata types in a current meadow.
- Given an interval query `Q`, `(<< // (>> /. Q))` returns the segment-local
  filename identifying its source.

The agent can then interpret each `@` record according to its `type`, associate
source-format records with `/` entries through `file` or their `/.` envelope,
recover the source of data through `//`, and use forager records to discover
derived annotation and ranking views. This is the bootstrap contract: a new
consumer does not need an external schema or filename-extension guess to
determine how the meadow was populated.

The type-discovery query only returns records with an explicit `type`. When
reading an older meadow, an agent must also accept an `@` record with no `type`
as a legacy forager record.

Structural GCL queries operate directly on the metadata annotations. A C++
consumer that retrieves the full text of an `@` interval through
`Txt::translate(...)` must pass the index-facing representation through
`json_translate(...)` before parsing or displaying it as ordinary JSON.

Metadata about a file has a `:file:` field inside its `@` record. A query such
as

```text
(>> @ (>> :file: "/data/hdd3/Collections/msmarco/collection.tsv"))
```

can then return only the metadata records describing that file. The feature for
the normalized filename applies only to data objects, so
`(<< : /data/hdd3/Collections/msmarco/collection.tsv)` selects the ordinary
objects from that file rather than its metadata. JSON and TSV ingestion both
implement these conventions. Conversely,

```text
(<< // (>> /. Q))
```

returns the normalized filename stored at the beginning of the file segment
containing `Q`. For file-format metadata, that filename interval also has `/`;
for a data segment, it has only `//`.

## Metadata Records

Every newly written metadata record is logically a JSON object whose complete
interval is annotated with `@`. Its members receive normal colon-path JSON field
annotations, such as `:type:` and `:tag:`, while the object itself receives `@`
instead of the ordinary `:` root annotation. Keeping these root roles distinct
prevents metadata from appearing in ordinary data-object queries.

The top-level `type` member identifies the kind of annotations or source format
described by the record:

```json
{
  "type": "..."
}
```

The metadata object is an extensible, type-discriminated record:

- New records must contain an explicit string `type`.
- Type-specific keys define the remainder of the object.
- Additional keys are allowed.
- Readers should ignore keys they do not understand when they can still
  interpret the record safely.
- For compatibility with existing meadows, a missing `type` is exactly
  equivalent to `"type": "forager"`.

The generic `@` annotation supports discovery. The normal JSON field
annotations support typed queries such as `(>> @ (>> :type: "forager"))`.
Current writers use these types:

| `type` | Describes | Common fields |
| --- | --- | --- |
| `json` | A source interpreted as JSON records | `file` |
| `tsv` | A source interpreted as tabular records | `file`, `separator`, `header`, `columns` |
| `forager` | One derived annotation run | `name`, `tag`, `parameters` |

Metadata describing a file has an additional common contract:

- The JSON object contains a top-level `file` member.
- `file` contains the normalized file identity used by Meadowlark.
- The source-format metadata and a `//` copy of that identity share a `/.`
  source segment; that identity copy is also the canonical `/` interval.
- The normalized file-identity feature annotates the file's data objects, not
  the metadata interval or filename copy.
- Consumers find file metadata through `@` and `:file:` and find file contents
  through `:` and the normalized file-identity feature. Consumers can recover
  a containing segment's filename through `//` and `/.`.

### Forager Metadata

A forager metadata record describes one run of a derived annotation process.
Its core shape is:

```json
{
  "type": "forager",
  "name": "tf-idf",
  "tag": "none",
  "parameters": {
    "start": "86",
    "end": "523259267",
    "contents": ":1:",
    "container": ":",
    "id": ":0:",
    "stemmer": "porter"
  }
}
```

Forager metadata follows these conventions:

- `name` identifies the forager implementation and annotation family.
- `tag` identifies a particular output or statistics view within that family.
  New records use the literal tag `none` when callers omit the tag.
- `parameters` contains `start` and `end` for the processed address range.
- Parameter values are currently strings; parameters other than `start` and
  `end` depend on the forager.
- `contents` identifies the intervals whose text was processed and whose
  starts carry the per-term frequency annotations.
- `container` identifies the enclosing result object and defaults to `:`.
- `id` is optional. Consumers that produce external identifiers, such as TREC
  output, require it; ranking itself does not.
- Other top-level keys may be added.

Existing forager records contain `name`, `tag`, and `parameters` but omit
`type`. They remain valid and must be interpreted as `type = "forager"`.
`json2forager(...)` accepts a missing type, requires any explicit type to be
`forager`, and ignores unknown top-level keys.

Foraging is an annotation pass over already ingested data, not a file append.
Its metadata records therefore remain outside `/.` source segments and have no
associated `//` filename unless a future, separately designed convention says
otherwise.

Legacy TF-IDF metadata was annotated with a lookup feature derived from its
name and empty tag:

```text
@tf-idf:
```

New metadata does not add a type-specific `@tf-idf:...` feature. It is found
through its structured fields. The base TF-IDF query is:

```text
(>> (>> @ (>> :type: "forager")) (>> :name: "tf-idf"))
```

Selecting the default new view adds its literal tag:

```text
(>> (>> (>> @ (>> :type: "forager")) (>> :name: "tf-idf"))
    (>> :tag: "none"))
```

The metadata record is a manifest for the run. The numeric results live under
related feature namespaces, including:

```text
tf-idf:none:tf:<term>
tf-idf:none:df:<term>
tf-idf:none:total:items
tf-idf:none:total:length
```

`TfIdfStats` uses the selected tag to find the corresponding metadata, validate
its `name` and `tag`, recover its queries/tokenizer/stemmer, and access the same
tagged statistics. For compatibility, an empty requested tag first selects the
first legacy `@tf-idf:` record. If none exists, the request means the literal
new tag `none` and uses the structured metadata query. An explicit nonempty tag
uses only the structured query; legacy nonempty tags are not part of the
compatibility contract. BM25 is selected separately as a ranking method; the
tag selects which TF-IDF statistics view BM25 consumes.

New forager metadata uses `contents`. `TfIdfStats` accepts legacy `gcl` when
`contents` is absent, and falls back to the resolved `container` when neither
is present. `contents` wins if both keys occur. No default is invented for
`id`.

On 2026-07-21, the user verified this compatibility behavior against the older
`b.meadow` and `c.meadow` indexes, whose metadata predates the current field
names. Both remained usable with the current reader.

The unresolved uniqueness policy for repeated `(name, tag)` runs is recorded in
`ai/improvements.md`.

### JSON Metadata

A source ingested as JSON has a small file metadata record:

```json
{
  "type": "json",
  "file": "./whatever"
}
```

The `file` value is the normalized source identity, not necessarily the string
originally passed by the caller. The metadata declares how Meadowlark
interpreted the source, so consumers do not have to infer its representation
from a filename extension.

The type is `"json"`, rather than `"jsonl"`, because it describes the JSON
annotation model produced in the meadow. JSON Lines is the current input
framing used by `append_jsonl(...)`; a separate field can record physical
framing later if a consumer needs that distinction.

The metadata object is annotated with `@`; its members have colon-path field
annotations, while its root does not have `:` or the filename feature. The
original Warren transaction begins with the normalized filename annotated by
both `/` and `//`; `/.` wraps that filename and the metadata. Each nonempty JSON
worker begins with its own `//` filename and wraps that filename and its records
with `/.`. All of those transactions are committed atomically.
Transaction-neutral `json_append(...)` writes the encoded JSON structure and
accepts the root feature separately from its colon-based member paths.

### TSV Metadata

The TSV metadata type is `"tsv"`. It describes the interpretation of a
TSV source, including the mapping from source headers to emitted annotation
features. Its shape is:

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
original header and the feature actually emitted by ingestion. The `index`
member is a JSON number.

When a header is present, its column annotations must not contain whitespace so
that they remain usable as raw GCL features. Historical Meadowlark behavior
replaced header whitespace with `_` and wrapped the result in colons, for
example:

```text
Favorite Food -> :Favorite_Food:
Animal        -> :Animal:
```

Header-derived labels replace each run of whitespace with `_`, preserve all
other characters, and wrap the result in colons. Features themselves may be
arbitrary strings; this normalization only avoids the known ambiguity of
whitespace in raw GCL. Punctuation such as `/`, `(`, and `)` is preserved even
when it may be awkward in a query. Empty headings, duplicate normalized labels,
and columns beyond the header fall back to their numeric column feature. The
header defines the recorded mapping: shorter rows have missing fields, while
unexpected wider rows are accepted and their additional fields receive numeric
features without a preliminary full-file scan. The recorded `columns` array is
the schema inferred from the first record; lazily tolerated extra columns do
not retroactively modify the metadata record.

When no header is declared, every column uses its numeric `:0:`, `:1:`, and
similar feature, and the column metadata omits the per-column `header` member.
The first record is then an ordinary `:` data object. When a header is declared,
the first record is annotated with `::` rather than `:` but remains part of the
file contents selected by the source-identity feature.

The TSV JSON is stored through transaction-neutral `json_append(...)`
with an `@` root and ordinary colon-path member annotations. Its root must not
receive the filename feature. As with JSONL, the source/metadata transaction
and every nonempty data worker begin with a `//` filename and receive a `/.`
envelope; only the canonical source/metadata filename also receives `/`. The
metadata, source marker, and TSV data are committed atomically. TSV remains a
direct Warren ingestion path and does not add a type-specific lookup annotation
beyond its structured fields.

It is acceptable for this capability to exist first as a library API even when
no command-line application exposes the `header` argument.

## Source Identity And Restart

Restartability needs a durable identity string for each append operation.
File-based appends currently use a normalized filename. Non-file appends must
receive an explicit source label or append id; otherwise duplicate detection is
not well-defined.

The durable marker/content identity mechanism has four annotation forms:

1. The canonical normalized source string is appended as text and annotated
   with `/` and `//`.
2. Each nonempty data transaction has another normalized source string
   annotated with `//`.
3. `/.` wraps each filename copy and its transaction-local metadata or data.
4. The featurized source string annotates the content written from that source.

Source-format metadata separately repeats the identity in its `file` field, as
described above.

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
is prefixed with `./`; other strings are unchanged. Possible stronger source
identity canonicalization is recorded in `ai/improvements.md`.

## Common Append Lifecycle

Append operations should share one lifecycle while writing through their
narrowest appropriate abstraction:

1. Establish or receive the durable source identity.
2. Start the original Warren at the top of the append operation.
3. Use that started Warren as the read snapshot and clone source.
4. Open a transaction for the canonical `/` and `//` source marker, source
   metadata, and their `/.` envelope.
5. Create worker clones from the started Warren.
6. In each worker that consumes data, write a `//` source string first, append
   through the Warren or clone, and wrap the local name and data with `/.`.
   Transaction-neutral helpers may share record encoding without owning the
   transaction.
7. Ready the source transaction and every worker transaction.
8. If any setup, write, or ready step fails, abort every transaction and end
   every clone.
9. On success, publish all transactions through the highest applicable batch
   helper.
10. End every clone and then the original Warren on every return path.

Direct Warren paths commit through `Warren::commit_all(...)`, which can discover
coordinated Bigwig publication. Meadowlark should not inspect or depend on the
concrete Warrens or their shared working directory.

Duplicate preflight may be performed by a batch caller such as the Meadowlark
CLI. The public append APIs and their eventual non-file variants still need a
deliberate contract for whether they also provide independent idempotence.

## Implemented Append State

### JSONL Files

`append_jsonl(warren, filename, ...)` is the current model for coordinated
append publication:

- It starts the original Warren and ends it through one top-level finish path.
- It streams plain or gzipped input with `maybe_zipped(...)`.
- It writes and readies the source marker and JSON metadata through the
  original Warren, with a `/.` envelope and a filename carrying `/` and `//`.
- It creates workers by cloning the already-started Warren.
- It writes JSON records through `json_append(...)` on direct Warren clones.
- Each nonempty worker writes a leading `//` filename and wraps it and its JSON
  records with `/.`.
- It annotates each nonempty JSON record with the source-identity feature.
- It readies all worker Warrens.
- It aborts the source and workers together on failure.
- It publishes the source transaction and workers through
  `Warren::commit_all(...)` and ends every clone and the original Warren.
- It emits `type = "json"` file metadata with the normalized source identity,
  using `@` as the root and colon paths for its fields. The filename feature
  annotates only JSON data records.

The CLI performs duplicate preflight before calling this function. The append
function itself does not currently repeat that check.

### TSV Files

`append_tsv(warren, filename, ...)` follows the coordinated lifecycle:

- It streams plain or gzipped input with `maybe_zipped(...)`, using the same
  synchronized record-reader pattern as JSONL.
- It buffers only the first record to derive the recorded column mapping from
  the header or first row; it does not pre-scan the remaining rows.
- It writes and readies the source marker and `type = "tsv"` metadata through
  the original Warren, with a `/.` envelope and a filename carrying `/` and
  `//`.
- It creates direct Warren transactions by cloning the started original.
- Its workers parse and append one TSV record at a time from the shared stream.
- Each nonempty worker writes a leading `//` filename and wraps it and its TSV
  records with `/.`.
- It annotates ordinary rows with `:` and an optional header row with `::`.
- It uses header-derived features when requested and numeric features otherwise.
- It annotates file contents, but not metadata, with the filename feature.
- It aborts the original and every worker together on failure.
- It publishes the original and all workers through `Warren::commit_all(...)`.

## Planned Append Surfaces

### JSONL From Strings

Add an append operation for an array or vector of JSON strings. It should share
the JSON record worker body and direct-Warren lifecycle with file JSONL while
taking an explicit source identity from its caller.

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

## Next Design Area

The JSONL and TSV file append lifecycles are coordinated. The next append
surface has not been selected. JSONL from strings, raw text files, and code as
raw text remain possible later steps and require separate discussion and
explicit coding approval.
