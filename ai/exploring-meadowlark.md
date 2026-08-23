# Exploring A Meadowlark Database

This guide is intended for a model or program connecting to an unfamiliar
Meadowlark database. Do not assume a schema from filenames or extensions. Let
the database describe itself.

## Start With Metadata

Run:

```text
@
```

Each result is a JSON metadata record. Read every record before deciding what
the database contains. Current records have a top-level `type`. To list the
explicit type values directly, run:

```text
(<< :type: @)
```

An older `@` record may have no `type`. Treat that record as if it contained
`"type":"forager"`.

Known current types are:

- `json`, `tsv`, `text`, and `code`: describe an ingested file.
- `forager`: describes a derived annotation pass, such as TF-IDF statistics.

The type vocabulary can grow. Preserve and inspect unknown records rather than
discarding them merely because their type is unfamiliar.

## Inventory The Sources

Run:

```text
/
```

This returns each canonical source filename. Current file metadata repeats
that identity in its `filename` field. To select metadata for a particular
source:

```text
(>> @ (>> :filename: "src/foo.cc"))
```

Do not infer the format from `.cc`, `.tsv`, or another suffix; use the metadata
record's `type`.

Older source metadata uses `file` instead of `filename`; query `:file:` when
exploring such a meadow.

Current filename text is internally framed as a JSON string, so display tools
show names with quotes and preserve leading `./` or `/`. The corresponding
filename feature is still the unquoted normalized path. Historical databases
may display their raw filename text without quotes.

## Learn The Data Shape

Ordinary data objects use `:`. JSON member and TSV column annotations use
colon-path features. A TSV metadata record's `columns` array is the authoritative
mapping from source columns to those features. If `header` is true, the header
record itself uses `::`.

Token-bearing text and code files are each one `:` object. Token-bearing
physical code lines use `#`; the annotation value is the one-based physical
line number. Blank or otherwise tokenless lines still count but have no `#`
interval. A completely tokenless file has no `:` object.

To restrict ordinary objects to a source, use its filename as a feature:

```text
(<< : src/foo.cc)
```

A bare filename query returns the addressable data chunk or chunks for that
source. JSONL and TSV may have more than one because ingestion workers publish
separate chunks. The containment query above returns the individual `:` objects
inside those chunks. A tokenless source has no filename-feature result even
though it remains present in `/` and `@`.

## Recover Provenance

Given any query `Q` that returns data, recover the source filename with:

```text
(<< // (>> /. Q))
```

For example:

```text
(<< // (>> /. (>> # hazel)))
```

returns the source files containing code lines that match `hazel`. The `#`
results themselves carry physical line-number values.

## Understand Derived Annotations

A no-filename `forager` metadata record defines an annotation layer. Read its:

- `name`, which identifies the annotation family;
- `tag`, which selects a particular view;
- top-level `query`, which identifies the intervals processed; and
- `parameters`, which describe the forager-specific interpretation.

Other parameters are forager-specific. For a current TF-IDF record, inspect:

- `parameters.container`, which identifies enclosing result objects; and
- optional `parameters.id`, which supplies external identifiers when needed.

Current writers use the literal tag `none` when no tag was supplied. Older
TF-IDF records may omit `type`, put the query in `parameters.contents` or
`parameters.gcl`, include `start` and `end`, or use the legacy `@tf-idf:`
lookup annotation.

Current file-oriented foraging also writes one completion record per processed
file. It has `type`, `filename`, `name`, and `tag`, but no `query` or
`parameters`; those come from the matching primary definition. All forager
metadata sits outside `/.` file segments.

File `@` records also sit outside `/.`. A `/.` interval is specifically a
nonempty data segment containing its local `//` filename and data. A tokenless
source still appears under `/` and `@` and has one `//`, but has no `/.` or `:`
interval. Older databases may instead place file metadata inside `/.` and may
put `//` on the canonical `/` filename; consumers should tolerate both layouts.

## Minimal Exploration Sequence

For a fresh connection, use this order:

1. Run `@` and inspect the full metadata records.
2. Run `(<< :type: @)` to summarize explicit metadata types.
3. Run `/` to inventory sources.
4. Use each file record's `type`, `filename`, and type-specific fields to
   construct data queries; accept historical `file` as a reader.
5. Use no-filename forager definitions to discover ranking or
   derived-annotation views; treat filename-bearing records as completion
   provenance.
6. For any result interval, use `(<< // (>> /. Q))` to identify its source.

The complete format contract and compatibility rules are in
`ai/meadowlark.md`.
