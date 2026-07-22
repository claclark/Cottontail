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

This returns each canonical source filename. File metadata repeats that
identity in its `file` field. To select metadata for a particular source:

```text
(>> @ (>> :file: "src/foo.cc"))
```

Do not infer the format from `.cc`, `.tsv`, or another suffix; use the metadata
record's `type`.

## Learn The Data Shape

Ordinary data objects use `:`. JSON member and TSV column annotations use
colon-path features. A TSV metadata record's `columns` array is the authoritative
mapping from source columns to those features. If `header` is true, the header
record itself uses `::`.

Text and code files are each one `:` object. Token-bearing physical code lines
use `#`; the annotation value is the one-based physical line number. Blank or
otherwise tokenless lines still count but have no `#` interval.

To restrict ordinary objects to a source, use its filename as a feature:

```text
(<< : src/foo.cc)
```

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

A `forager` metadata record is a manifest for annotations added after ingestion.
Read its:

- `name`, which identifies the annotation family;
- `tag`, which selects a particular view;
- `parameters.start` and `parameters.end`, which delimit the processed range.

Other parameters are forager-specific. For a current TF-IDF record, inspect:

- `parameters.contents`, which identifies the intervals processed;
- `parameters.container`, which identifies enclosing result objects; and
- optional `parameters.id`, which supplies external identifiers when needed.

Current writers use the literal tag `none` when no tag was supplied. Older
TF-IDF records may omit `type`, use `gcl` instead of `contents`, or use the
legacy `@tf-idf:` lookup annotation.

Foraging is not a file append. A forager record generally has no associated
filename and sits outside `/.` file segments.

## Minimal Exploration Sequence

For a fresh connection, use this order:

1. Run `@` and inspect the full metadata records.
2. Run `(<< :type: @)` to summarize explicit metadata types.
3. Run `/` to inventory sources.
4. Use each file record's `type`, `file`, and type-specific fields to construct
   data queries.
5. Use forager records to discover ranking or derived-annotation views.
6. For any result interval, use `(<< // (>> /. Q))` to identify its source.

The complete format contract and compatibility rules are in
`ai/meadowlark.md`.
