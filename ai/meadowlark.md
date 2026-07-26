# Meadowlark Database Conventions

Status date: 2026-07-25.

This is the durable reference for Meadowlark's machine-facing structure,
metadata, source provenance, and append invariants. Completed implementation
history belongs in `ai/log.md`; possible future changes belong in
`ai/improvements.md`.

## Discovery Contract

A current Meadowlark database is intended to describe itself. A new consumer
should inspect metadata rather than infer structure from filenames, extensions,
or application-specific knowledge.

The basic discovery queries are:

```text
@
(<< :type: @)
/
```

These return all metadata records, their explicit type values, and all
canonical source filenames, respectively.

Start with `@`. Each record is a typed JSON manifest describing either an input
source or a derived annotation pass. Then use `/` to inventory the source
files. A missing `type` is the one compatibility exception: it means
`"type":"forager"`.

`ai/exploring-meadowlark.md` is a short, standalone exploration guide suitable
for giving directly to a model connected to a database.

## Structural Feature Vocabulary

Meadowlark reserves short punctuation features for its machine-facing schema.
Ordinary words and names remain available to source data and applications.

| Feature | Meaning |
| --- | --- |
| `@` | A complete metadata record. |
| `/` | The canonical source filename, written once for a file. |
| `//` | A segment-local copy of a source filename. |
| `/.` | A file segment containing a `//` filename and local metadata or data. |
| `:` | An ordinary input object or record. |
| `::` | A TSV header record, when a header is requested. |
| `#` | A token-bearing physical code line; its value is the one-based line number. |

JSON member annotations use colon paths such as `:type:`, `:file:`, and
`:parameters:contents:`. These describe fields inside an object; they do not
change the object's root role. In particular, an `@` metadata record has field
annotations such as `:type:` but does not also receive the ordinary `:` root.

Features may be arbitrary strings. Punctuation-rich features and filenames
such as `./foo/bar.txt` are valid. ASCII whitespace is awkward in raw GCL and
should be avoided in generated feature labels.

Bare `#` is an ordinary feature query. The existing operator form `(# N)` is a
separate fixed-width GCL operation.

## Metadata Record Contract

Every newly written metadata record is logically an extensible JSON object.
Its complete interval receives `@`, and its members receive the usual JSON
colon-path annotations.

The top-level `type` string discriminates the record:

```json
{
  "type": "..."
}
```

The rules are:

- New records contain an explicit string `type`.
- The type determines the meaning of the remaining keys.
- Additional keys are allowed.
- Readers should ignore unknown keys when the understood part remains safe to
  interpret.
- A missing `type` means `forager` for compatibility with old databases. It
  does not mean an unknown file format.

Current types are:

| `type` | Meaning | Main fields |
| --- | --- | --- |
| `json` | A source represented as JSON objects | `file` |
| `tsv` | A tabular source and its column mapping | `file`, `separator`, `header`, `columns` |
| `text` | One unstructured text object | `file` |
| `code` | One line-addressable source-code object | `file` |
| `forager` | One derived annotation pass | `name`, `tag`, `parameters` |

The type vocabulary is open. Consumers must discover it from the records they
find rather than assume this table is permanently exhaustive.

When C++ code obtains the stored text of an `@` interval directly through
`Txt::translate(...)`, it must apply `json_translate(...)` before parsing or
presenting it as ordinary JSON. GCL structural queries operate on the indexed
annotations directly.

## File Metadata

Every file-format record contains its normalized source identity in `file`:

```json
{
  "type": "json",
  "file": "./records.jsonl"
}
```

`text` and `code` use the same shape with their corresponding type. The JSON
type describes the representation in the database; current JSON input happens
to use JSON Lines framing, so the metadata type is `json`, not `jsonl`.

The `file` field is authoritative. Consumers should not guess a source's type
from its extension.

### TSV Metadata

TSV metadata records the interpretation of the source and the mapping from
columns to emitted features:

```json
{
  "type": "tsv",
  "file": "./records.tsv",
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

`columns` is an ordered array so empty headings, duplicate headings, and
normalization collisions remain representable. `index` is a JSON number.

With a header:

- Each run of header whitespace becomes `_`.
- Other characters are preserved, even when punctuation makes raw GCL
  inconvenient.
- The result is wrapped in colons.
- Empty headings, duplicate normalized labels, and normalization collisions
  fall back to numeric features such as `:0:`.
- The header record receives `::`, not `:`.

Without a header, columns use `:0:`, `:1:`, and so on, and column entries omit
`header`.

The first record establishes the metadata mapping. Shorter later rows simply
have missing fields. Wider later rows are accepted and their extra fields use
numeric features, but they do not retroactively change the metadata record.
Ingestion does not scan the complete file in advance.

### Text And Code Data

A text or code file is one ordinary `:` object. Code is appended a physical
line at a time. Each physical line advances a one-based counter, including
blank or otherwise tokenless lines. A line that produced a valid token interval
receives `#` with that physical line number as its annotation value; a
tokenless line receives no `#` annotation.

Thus a code query can return the matching line interval and its physical line
number, while the normal provenance query recovers the file containing it.

## Forager Metadata

A forager record describes one derived annotation run:

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

The conventions are:

- `name` identifies the forager implementation and annotation family.
- `tag` selects a particular output or statistics view in that family. New
  writers use the literal `none` when no tag is supplied.
- `parameters` contains `start` and `end`, expressed as strings, for the
  processed address range.
- Other parameters depend on the forager, and other top-level keys are allowed.

For current TF-IDF metadata specifically:

- `contents` identifies the intervals whose text was processed and whose starts
  carry term-frequency annotations.
- `container` identifies the enclosing result objects and defaults to `:`.
- `id` is optional and has no default. Ranking does not require it; consumers
  producing external identifiers, such as TREC output, do.

Foraging annotates data already in the database. It is not a file append, so a
forager record is outside file `/.` segments and has no source filename.

For example, current TF-IDF records can be selected structurally with:

```text
(>> (>> @ (>> :type: "forager")) (>> :name: "tf-idf"))
```

The default current view additionally has `:tag:` equal to `"none"`. The
manifest explains the run; numeric results live in the related tagged feature
namespace, for example:

```text
tf-idf:none:tf:<term>
tf-idf:none:df:<term>
tf-idf:none:total:items
tf-idf:none:total:length
```

### Forager Compatibility

Readers preserve these older forms:

- An `@` record with no `type` is a forager record.
- An empty requested statistics tag first checks the legacy `@tf-idf:` lookup
  feature. If none exists, it means the current literal tag `none`.
- Explicit nonempty tags use the structured `@`, `:type:`, `:name:`, and
  `:tag:` lookup.
- Current metadata uses `contents`. Readers fall back to legacy `gcl`, then to
  the resolved `container`, when `contents` is absent.
- No compatibility path invents an `id`.

This compatibility was exercised against the older `b.meadow` and `c.meadow`
databases, which predate the current field names.

## Source Identity And Provenance

File ingestion connects metadata, data, and filenames in three ways:

1. The canonical normalized filename text receives `/` and `//` once. It is
   followed by the file's `@` metadata, and `/.` wraps that local filename and
   metadata.
2. Every nonempty data segment begins with another filename copy receiving
   `//`. A `/.` annotation wraps that filename and the local data.
3. The normalized filename is also featurized and annotates the data objects
   from the file. It does not annotate metadata or the filename copies.

JSONL and TSV may have several data segments because workers append coordinated
transactions. Text and code have one data segment. These are physical details;
the query conventions are the same.

Useful provenance queries are:

```text
/
(>> @ (>> :file: "src/foo.cc"))
(<< : src/foo.cc)
(<< // (>> /. Q))
```

These inventory canonical filenames, find metadata for one file, select that
file's ordinary objects, and recover the filename associated with `Q`,
respectively.

The last query is the standard inverse lookup: given any interval query `Q`,
find the `/.` segment containing it, then return that segment's leading `//`
filename. It works for ordinary objects, TSV records, and `#` code lines.

Current path normalization is deliberately small: a filename containing no
`/` is prefixed with `./`; other paths are unchanged.

## Publication And Restart Invariants

The canonical `/` marker must become visible atomically with the source
metadata and data. Otherwise restart could mistake a partial append for a
complete one.

The public `meadowlark::append_all(...)` operation declared in
`meadowlark/meadowlark.h` preflights its typed input plan through `/`, skips
sources already present, and dispatches missing inputs. The Meadowlark app is a
thin command-line caller of this library operation. The durable invariant is:

> Rerunning Meadowlark with the complete source list skips committed sources
> and safely retries missing or interrupted sources.

JSONL and TSV coordinate their source transaction and worker transactions.
Text and code use one transaction per complete file. A failure aborts the
affected coordinated append rather than publishing its canonical marker alone.

Any future file adapter must preserve the same contract: an explicit typed
metadata record, a canonical `/` identity, transaction-local `//` provenance
inside `/.`, a filename feature on data only, and coordinated publication.

## Create-Time Warren Parameters

The Meadowlark command accepts Warren parameter assignments immediately after
`--create` and before the first input flag:

```text
meadowlark --create convert:no --tsv collection.tsv
```

Each assignment has the form `ID:value`. `ID` begins with an ASCII letter and
then contains only letters, digits, `_`, or `-`; the value is nonempty and may
contain additional colons. An argument beginning with `-` starts the ordinary
input-option parser and is not accepted as a parameter assignment.

After creating the Bigwig, the app applies every assignment through
`Warren::set_parameter(...)` before calling `append_all(...)`. The operation
therefore updates both the live Fluffle parameter package and the burrow DNA.
These are Warren activation controls, not Meadowlark metadata records, and
they do not alter the `@`, `/`, `//`, or `/.` discovery contracts above.
