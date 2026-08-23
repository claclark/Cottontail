# Meadowlark Database Conventions

Status date: 2026-08-22.

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
| `/` | The canonical source filename and committed-source marker, written once for a file. |
| `//` | A segment-local copy of a source filename. |
| `/.` | A nonempty file-data segment containing a `//` filename and local data. |
| `:` | An ordinary input object or record. |
| `::` | A TSV header record, when a header is requested. |
| `#` | A token-bearing physical code line; its value is the one-based line number. |

JSON member annotations use colon paths such as `:type:`, `:filename:`, and
`:parameters:container:`. These describe fields inside an object; they do not
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
| `json` | A source represented as JSON objects | `filename` |
| `tsv` | A tabular source and its column mapping | `filename`, `separator`, `header`, `columns` |
| `text` | One unstructured text object | `filename` |
| `code` | One line-addressable source-code object | `filename` |
| `forager` | A derived annotation definition or file completion | `name`, `tag`, plus `query` or `filename` |

The type vocabulary is open. Consumers must discover it from the records they
find rather than assume this table is permanently exhaustive. `type` names the
kind of metadata record; it is not a separate generic action or verb field.

## Internal JSON Text And Conversion

Cottontail's indexed JSON representation is not external textual JSON. Objects,
arrays, strings, colons, commas, and number boundaries use reserved Unicode
noncharacters that the UTF-8 tokenizer treats as structural tokens. String
payloads contain decoded literal bytes, so a stored string may contain a
literal control character that external JSON would have to escape.

The two conversion operations have deliberately different contracts:

- `json_translate(...)` is a permissive, lossy display operation for arbitrary
  intervals. It replaces structural noncharacters with visible JSON
  punctuation, removes number-boundary markers, and handles literal CR and LF
  lazily as a pending space. The space is emitted only if later content follows,
  preventing token splices without exposing trailing text-store fluff. Other
  control bytes, quotes, and backslashes inside structural strings remain
  escaped for safe one-line display. The result is not validated and is not a
  transmission contract.
- `json_convert(...)` is the machine boundary. It requires one complete JSON
  value, converts the structural representation, performs the escaping required
  by external JSON—including literal CR and LF inside strings—and validates the
  result. Use it before parsing or transmitting a complete internally encoded
  JSON value, such as a JSON input record's `:` interval or any `@` metadata
  record. Ordinary text, code, and TSV `:` intervals are not JSON values.

`meadowlark::json2forager(...)`, and therefore `TfIdfStats`, uses
`json_convert(...)` for internally represented metadata while retaining direct
parsing of historical ordinary-JSON records. GCL structural queries operate on
the indexed annotations and need neither conversion. The unused `Txt::raw(...)`
escape hatch has been removed; unwrapped `Txt::translate(...)` remains the way
to obtain the index-facing representation, while a configured `JsonTxt` wrapper
continues to apply display translation at its boundary.

## File Metadata

Every newly written file-format record contains its normalized source identity
in `filename`:

```json
{
  "type": "json",
  "filename": "./records.jsonl"
}
```

`text` and `code` use the same shape with their corresponding type. The JSON
type describes the representation in the database; current JSON input happens
to use JSON Lines framing, so the metadata type is `json`, not `jsonl`.

The `filename` field is authoritative. Readers accept the historical spelling
`file`; consumers should not guess a source's type from its extension.

### TSV Metadata

TSV metadata records the interpretation of the source and the mapping from
columns to emitted features:

```json
{
  "type": "tsv",
  "filename": "./records.tsv",
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

A text or code file with addressable tokens is one ordinary `:` object. Code is
appended a physical line at a time. Each physical line advances a one-based
counter, including blank or otherwise tokenless lines. A line that produced a
valid token interval receives `#` with that physical line number as its
annotation value; a tokenless line receives no `#` annotation. A completely
tokenless file follows the source-only rule described below and has no `:`.

Thus a code query can return the matching line interval and its physical line
number, while the normal provenance query recovers the file containing it.

## Forager Metadata

A primary forager record defines one derived annotation layer:

```json
{
  "type": "forager",
  "name": "tf-idf",
  "tag": "none",
  "query": ":1:",
  "parameters": {
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
- `query` identifies the intervals processed and whose starts receive the
  derived annotations.
- `parameters` contains the forager-specific interpretation of those intervals.
- The complete `(name, tag, query, parameters)` specification is global and
  immutable for that layer.

For current TF-IDF metadata specifically:

- `container` identifies the enclosing result objects and defaults to `:`.
- `id` is optional and has no default. Ranking does not require it; consumers
  producing external identifiers, such as TREC output, do.

Foraging applies the layer one logical file at a time. Its worker annotations
and separate completion record form one coordinated commit set:

```json
{
  "type": "forager",
  "filename": "./records.tsv",
  "name": "tf-idf",
  "tag": "none"
}
```

The completion record deliberately has neither `query` nor `parameters`; they
come from the unique primary definition. Both forms are `@` metadata outside
file `/.` segments. The completion-marker transaction is ordered after the
worker transactions. `TfIdfStats` reads the primary definition only.

For example, the current TF-IDF primary definition can be selected
structurally with:

```text
(>> (>> (>> @ (>> :type: "forager")) (>> :name: "tf-idf")) :query:)
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
  `:tag:` lookup. Current readers require top-level `query` when selecting a
  primary so they do not select file completion records.
- Current metadata uses top-level `query`. Readers fall back to historical
  `parameters.contents`, then `parameters.gcl`, then to the resolved
  `container`, when it is absent.
- New file-oriented writers refuse to extend a historical interval-oriented
  layer with the same `(name, tag)`; old layers remain readable.
- No compatibility path invents an `id`.

This compatibility was exercised against the older `b.meadow` and `c.meadow`
databases, which predate the current field names.

## Source Identity And Provenance

File ingestion connects metadata, data, and filenames in three ways:

1. One canonical normalized filename receives `/`. The file's separate typed
   metadata object receives `@`; neither is inside `/.`.
2. Every nonempty data segment begins with another filename copy receiving
   `//`. A `/.` annotation wraps that filename and the local data.
3. The normalized filename is also featurized and annotates each nonempty
   worker's data interval. It does not annotate metadata or filename copies.
   A bare filename feature query therefore returns the file's addressable data
   chunks, while `(<< : filename)` returns the ordinary objects inside them.

The text underlying new `/` and `//` intervals uses Cottontail's internal JSON
string delimiters. `json_translate(...)` therefore displays a complete name as
`"src/foo.cc"`, including leading punctuation such as `./` or `/`. This
framing changes only the stored filename text; the filename feature remains the
unquoted normalized path used in queries such as `(<< : src/foo.cc)`.

JSONL and TSV may have several data segments because workers append coordinated
transactions. Each nonempty worker has one filename-feature interval spanning
its data, while individual JSON records and ordinary TSV data rows continue to
receive `:`; a requested TSV header receives `::`. Text and code have one data
segment. These are physical details; the query conventions are the same.

A zero-byte or otherwise tokenless source still publishes `/`, `@`, and one
local `//`. It publishes no `/.`, `:`, or normalized-filename interval because
there is no addressable data interval. Non-token text remains retrievable with
the preceding filename token according to the normal text-store translation
rules. The canonical `/` marker is still written so restart cannot repeatedly
retry a successfully processed empty or dust-only source. Zero-record JSONL and
TSV sources write one local `//`, not one copy per idle worker.

Useful provenance queries are:

```text
/
(>> @ (>> :filename: "src/foo.cc"))
(<< : src/foo.cc)
(<< // (>> /. Q))
```

These inventory canonical filenames, find current metadata for one file,
select that file's ordinary objects, and recover the filename associated with
`Q`, respectively. Use historical `:file:` when exploring an older meadow.

The last query is the standard inverse lookup: given any interval query `Q`,
find the `/.` segment containing it, then return that segment's leading `//`
filename. It works for ordinary objects, TSV records, and `#` code lines.

Current path normalization is deliberately small: a filename containing no
`/` is prefixed with `./`; other paths are unchanged.

Readers may encounter older databases in which filename text is unframed,
canonical `/` also receives `//`, `@` is inside `/.`, or JSON filename features
are record-sized. These historical layouts remain readable. Restart discovery
applies display translation and accepts both raw and newly framed `/` filename
text by trimming surrounding whitespace and, when present, one pair of outer
double quotes. This covers ordinary paths. Filenames containing bytes that
display translation itself escapes, notably literal double quotes or
backslashes, remain the known unusual ambiguity in this simple restart rule.

## Publication And Restart Invariants

The canonical `/` marker is the durable completion marker for source
ingestion. JSONL and TSV order its source transaction after their worker data
transactions in the coordinated commit set so ordinary restart does not treat
unpublished work as complete.

The public `meadowlark::append_all(...)` operation declared in
`meadowlark/meadowlark.h` preflights its typed input plan through `/`, skips
sources already present, and dispatches missing inputs. The Meadowlark app is a
thin command-line caller of this library operation. The durable invariant is:

> Rerunning Meadowlark with the complete source list skips committed sources
> and safely retries missing or interrupted sources.

JSONL and TSV coordinate their source transaction and worker transactions.
Text and code use one transaction per complete file. A failure before
publication aborts the affected coordinated append rather than publishing its
canonical marker alone; interrupted publication is handled by the Bigwig/Fiver
recovery path. Committed data is visible to a newly started read epoch; an
already-started Warren retains its existing view. Restart checks therefore run
in a fresh started read epoch.

`Warren::commit_all(...)` currently publishes its already-readied Fivers
sequentially. A read epoch starting during that short window can observe a
proper subset, usually worker data before the final source or forager marker.
Recovery still resolves the coordinated set, and ordinary Meadowlark command
use does not overlap operations this way. A publication gate for newly starting
readers is recorded as a possible improvement in `ai/improvements.md`.

Any future file adapter must preserve the same contract: an explicit typed
metadata record, a canonical `/` identity, a local `//`, a filename feature on
addressable data only, and coordinated publication. For nonempty data, `//`
and the data are enclosed by `/.`; for an empty or tokenless source, `//`
remains without `/.` or a filename-feature interval.

## Compatibility Verification

After the 2026-08-21 filename and metadata adjustment, the user verified the
reader and writer in several ways: indices dating from 2022, newer Hazel/Fiver
indices, a complete build from scratch, and the existing 1.3 TB ClimbMix index
all worked without observed problems. These checks exercise both historical
read compatibility and current-format construction; they do not rewrite old
indices into the new metadata layout.

After the 2026-08-22 file-oriented forager change and its follow-up coverage,
the user reports that the complete regression suite and additional tests pass.
The user also verified current MS MARCO construction and ranking and continued
to query older indices successfully.

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
