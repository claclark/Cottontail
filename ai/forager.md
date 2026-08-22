# File-Oriented Foraging Plan

Status date: 2026-08-22.

This document records the agreed direction for replacing Meadowlark's
interval-run foraging interface with file-oriented execution. It is an
implementation plan, not coding authorization. The user runs regression tests
and makes commits; agent verification is compile-only unless explicitly
changed.

## Purpose

A forager adds a derived annotation layer to content already stored in a
Meadowlark database. The query says what intervals receive the operation. A
filename says which logical file owns those intervals and therefore supplies
the atomic unit for application, retry, and deletion.

For a concrete file, the effective query is the containment expression
conceptually written as `(<< query normalized-filename)`. Only results from
that file-scoped query are handed to workers. A result that crosses the
indexed boundary represented by the filename feature is not part of that
file's forage operation.

The central invariants are:

- `(name, tag)` uniquely identifies one forager layer in a meadow.
- The layer's query and parameters are global and immutable for that pair.
- Every worker interval comes from the query scoped by exactly one normalized
  filename feature.
- A separate file-specific record says only that the layer has been applied to
  that file.
- All annotations and the completion record for one file publish atomically.
- Deleting a file can remove its content, derived annotations, aggregate
  contributions, and file-specific metadata together.
- Old databases remain readable and rankable. New writers do not attempt to
  extend an old interval-oriented layer.

The design deliberately retains queries. Files scope a forager query; they do
not replace it.

## Terminology And Normalization

The command-line and API call the GCL expression being foraged `query`. In the
new metadata format it is also the top-level `query` field. It is not stored in
`parameters`.

For concrete Forager factories, `recipe` means the forager tag. This follows
the existing `TfIdfStats` convention; it does not give `recipe` a second
meaning as the query.

An omitted tag canonicalizes to the literal `none`. Consequently, an omitted
tag and an explicit `none` refer to the same layer and annotation namespace.
The default current TF-IDF namespace remains `tf-idf:none:...`. Historical
empty-tag TF-IDF annotations under `tf-idf:...` remain a reader compatibility
case.

All public file-oriented operations apply Meadowlark's filename normalization.
In particular, a name containing no slash receives the existing `./` prefix.
Durable metadata records the normalized name.

A filename selector is either a literal logical filename or a filesystem-style
pattern matched against the meadow's normalized `/` inventory. Selectors are
interpreted by the library so command-line and Python callers use the same
path. They are not expanded against the host filesystem.

## Metadata Model

### Common File Association

New file-specific metadata uses `filename` as a common top-level field beside
`type`:

```json
{
  "type": "json",
  "filename": "./foo.json"
}
```

JSON, TSV, text, code, forager-completion, and future file-specific metadata
share this convention. Existing source metadata uses the older field `file`;
readers that need source type information must accept both `filename` and
`file`. Old records are not migrated.

The `@` record remains separate from `/.` file data and is not annotated with
the normalized-filename feature. Its indexed `:filename:` member provides the
metadata-to-file association.

### Primary Forager Definition

One no-filename `@` record defines each `(name, tag)` pair:

```json
{
  "type": "forager",
  "name": "tf-idf",
  "tag": "foo",
  "query": ":paragraph:",
  "parameters": {
    "container": ":",
    "id": ":id:",
    "stemmer": "porter"
  }
}
```

The primary definition is the sole authority for the layer's query and
parameters. Reusing the pair requires exact query and parameter equality.
Semantically equivalent but textually different queries are still different
definitions and require another tag.

The definition is committed before applying the layer to any file. A valid
definition with no file completion records is allowed: it means that the layer
has been declared but has not yet been applied anywhere.

### Per-File Completion Record

Successful application to a file writes:

```json
{
  "type": "forager",
  "filename": "./foo.json",
  "name": "tf-idf",
  "tag": "foo"
}
```

It deliberately omits `query` and `parameters`; those come from the unique
primary definition. This record is a durable completion marker, not ranking
configuration.

`TfIdfStats` reads only the primary definition. File completion records are
used for restart/skip behavior, browsing, provenance, and eventual file
deletion.

## Public Library Surface

Replace the existing public GCL/range/vector overload family with a small
file-oriented surface along these lines:

```cpp
bool already_foraged(
    std::shared_ptr<Warren> warren,
    const std::string &filename,
    const std::string &name,
    const std::string &tag,
    bool *foraged,
    std::string *error = nullptr);

bool forage(
    std::shared_ptr<Warren> warren,
    const std::string &filename,
    const std::string &query,
    const std::string &name,
    const std::string &tag,
    const std::map<std::string, std::string> &parameters,
    std::string *error = nullptr,
    size_t threads = 0);

bool forage_all(
    std::shared_ptr<Warren> warren,
    const std::vector<std::string> &filenames,
    const std::string &query,
    const std::string &name,
    const std::string &tag,
    const std::map<std::string, std::string> &parameters,
    std::string *error = nullptr,
    size_t threads = 0);
```

The `forage_all(...)` vector contains filename selectors, not necessarily
already-expanded concrete names. The library expands them against the meadow's
`/` inventory, normalizes and deduplicates the results, and then performs the
operation. An empty vector means all files that contain at least one result
for the file-scoped query. A nonempty selector that matches no meadow file is
an error; it must never collapse into the empty-vector meaning of "all files."

`already_foraged(...)` mirrors `already_appended(...)`: it operates on a
started read view so `forage_all(...)` can preflight a batch without repeatedly
starting the Warren. It checks the normalized filename plus canonical name and
tag. A source `/` marker does not mean that any forager has been applied.

The singular public operation owns the top-level lifecycle for one file.
`forage_all(...)` owns one top-level started Warren and uses a private
already-started per-file helper rather than nesting the public lifecycle.

The parameter-bearing operations define a new specification or verify an
existing one. A corresponding parameter-omitting path requires the primary
definition to exist and uses its stored parameters. The exact overload spelling
can be chosen during implementation, but omission must not be represented by a
default empty map: an explicitly supplied empty map is a legitimate parameter
set.

## Forager Class Boundary

The basic class idea remains valid: one stateful worker receives query-result
intervals, writes derived annotations, accumulates worker-local summaries, and
flushes those summaries when its assigned work is complete.

The runtime worker should not know about:

- creating or publishing metadata definitions or completion records;
- filename discovery or scoping;
- query iteration and work division;
- transaction creation, commit, abort, or coordinated publication;
- retry and skip policy.

These responsibilities move to the file-oriented coordinator.

The current `Committable` inheritance, `label()`, transaction delegation,
mutex, and vector/hopper/range iteration overloads embody the old assumption
that a Forager instance represents a complete independently committed run.
They should be removed or reduced. The useful worker interface is conceptually:

```cpp
static bool check(started_warren, query, name, tag, parameters, error);
static std::shared_ptr<Forager>
make(started_worker_warren, name, tag, parameters, error);

bool forage(addr p, addr q, error);
bool finish(error);
```

The exact finalization name may be chosen during implementation. Its meaning is
only "write this worker's accumulated annotations"; it must not ready or commit
the Warren.

There is also a useful existing-definition factory on each concrete forager:

```cpp
TfIdfForager::make(started_warren, recipe, error);
```

Here `recipe` is the tag. The factory finds the unique current primary record
for its known forager name, obtains the stored parameters, and delegates to the
same parameter-validating construction path. It fails if the definition is
missing or legacy. This factory may read metadata but never writes it; the
worker it returns has no metadata-management responsibility. A generic
`Forager` dispatcher still needs the name in addition to the recipe, or the
coordinator must dispatch to the concrete subclass first.

### Validation

This narrow `Forager::check(...)` remains useful when it receives the actual
started Warren and the complete specification:

```cpp
static bool check(
    std::shared_ptr<Warren> warren,
    const std::string &query,
    const std::string &name,
    const std::string &tag,
    const std::map<std::string, std::string> &parameters,
    std::string *error = nullptr);
```

It performs no writes and has no metadata knowledge. It should:

1. Parse/compile the GCL query against the started Warren.
2. Dispatch the named forager.
3. Confirm that the implementation can be constructed with the requested tag
   and parameters in this Warren.

Subclass factories validate the parameters they consume while constructing
their worker state, such as TF-IDF's tokenizer, stemmer, and tagged
featurizers. They do not write metadata. The validation path should reuse that
construction logic rather than duplicate it.

The broader family of rarely useful `check(...)` methods elsewhere in
Cottontail is a possible future cleanup, not part of this change.

### Worker Finalization

`TfIdfForager` continues to place per-term TF annotations at the start of each
query interval. Its worker-local DF, item-total, and length-total annotations
are written during finalization at the worker's minimum processed address.

Every worker instance must process intervals from only one file. This keeps all
of its aggregate contributions anchored inside that file so deletion removes
them naturally. Finalization no longer calls `Warren::ready()`; the coordinator
does so after all forager annotations have been written.

The current question of whether stored TF-IDF DF annotations remain useful is
separate from this structural change.

## Definition Validation And Legacy Refusal

Before publishing a new definition, the coordinator starts the Warren and runs
`Forager::check(...)`. Invalid query, tokenizer, stemmer, tag-prefix, or other
implementation setup must fail before durable metadata is written.

When parameters are supplied, it then looks up the canonical `(name, tag)`:

1. No primary record exists: write and commit the new definition.
2. A current primary record exists: require exact query and parameter equality.
3. An older interval-oriented record exists for the same logical pair: refuse
   to extend it.

When parameters are omitted, the current primary record must already exist.
The supplied query must exactly match its query, and the concrete
`make(warren, recipe, error)` path uses its stored parameters. Missing and
legacy definitions are errors. This distinction is represented by an overload
or another explicit optional form, not by treating an empty parameter map as
omission.

The uniqueness check is semantic preflight, not a new cross-process locking
mechanism. As with the standard Meadowlark commands, users must not
deliberately race incompatible definitions for the same `(name, tag)`.

Reader compatibility and writer compatibility are deliberately different. Old
databases remain queryable and rankable, but new file-oriented writers do not
migrate old metadata or infer per-file completion from historical address
ranges.

For TF-IDF, detection follows the paths already established in
`tf-idf_stats.cc`:

- For the default tag, probe the historical `@tf-idf:` lookup feature and
  treat legacy empty tag as current logical tag `none`.
- Parse literal historical JSON through the existing `json2forager(...)`
  compatibility path.
- Also inspect structurally indexed `type=forager`, `name=tf-idf`, and matching
  `tag` records from the later interval-run format.
- `start`/`end`, legacy `gcl`, or the absence of the new top-level `query` on a
  matching run manifest are evidence of the old writer. The intermediate
  `parameters.contents` form is also reader-only.

The error should explain that the database remains readable but the historical
layer cannot safely be extended. A different new tag may define an independent
file-oriented layer.

## File And Query Selection

The query defines the intervals handed to the worker. For each concrete file,
the coordinator evaluates the semantic equivalent of:

```text
(<< query normalized-filename)
```

The normalized filename is the indexed feature that marks the file's data
chunks. Only results wholly contained by one of those intervals are returned;
queries do not forage across those boundaries. Examples of the unscoped query
and ranking parameters are:

```text
TSV:   query=:1:          container=:   id=:0:
JSON:  query=:paragraph:  container=:   id=:id:
text:  query=:            container=/.  id=//
```

`container` and optional `id` remain parameters. `id` has no invented default.
They describe ranking interpretation; they do not replace the query.

For an explicitly selected filename, zero scoped query results is still a
successful application and should publish its completion record so retry does
not loop forever. With an empty selector list, files with no scoped query
result are not selected in the first place.

`forage_all(...)` accepts filesystem-style selectors directly. It enumerates
`/`, decodes both current framed names and historical raw names, normalizes
selectors consistently, matches against the meadow inventory, and
deduplicates concrete names. Each selector in a nonempty input must match at
least one file. Both the command-line application and the Python wrapper pass
their selector strings to this same library path. Indexed-regular-expression
work remains unrelated and deferred.

## Per-File Transaction And Retry Lifecycle

The atomic unit is one logical file, not one worker and not the complete
multi-file invocation.

For one selected file:

1. Resolve `(<< query normalized-filename)` and use only its results.
2. Create the required worker Warrens and transactions.
3. Construct one Forager instance per worker Warren.
4. Give every worker only intervals from this file.
5. Write the file-specific completion record in a transaction coordinated with
   those workers.
6. Run each worker's finalization hook.
7. Ready every worker Warren and the completion-record Warren.
8. Abort all of them if setup, interval processing, finalization, or ready
   fails.
9. Publish them together through `Warren::commit_all(...)`.

A marker-only transaction is valid when an explicitly requested file contains
no matching query interval. A completion record is never visible without all
derived annotations for that file, and derived annotations are never visible
without the completion record.

A multi-file invocation may stop after earlier files have committed. This is
expected. Repeating the same invocation skips their completion records and
continues with unfinished files.

## `forage_all(...)` Preflight And Scheduling

`forage_all(...)` starts the Warren for the complete operation, validates or
creates the global definition, expands the supplied selectors, and preflights
all normalized concrete files in one read view. An empty selector vector
discovers all files with at least one scoped query result. A nonempty vector
selects the union of its literal or patterned matches, including explicit
files with zero scoped results. Each selected file is classified as:

- no `/` source marker: error for an explicitly named file;
- matching file completion record: skip;
- source exists without completion: schedule it.

Initial concurrency should follow the already-understood `append_all(...)`
model rather than introduce a new performance policy:

- JSON and TSV currently contain many ordinary objects per file. Process one
  such file at a time and parallelize its query intervals across workers.
- Text and code currently contain one ordinary object per file. Process
  several files concurrently with one worker per file.
- Keep the overall worker count within the requested thread budget.
- Look up source type through new `filename` metadata while accepting existing
  source records that use `file`.

Once the formats and lifecycle are stable, file-size-aware scheduling and other
performance experiments can be considered. Future logical file types may need
a different scheduling classification.

## Command-Line Interface

Change `apps/forage.cc` to:

```text
usage: forage [--meadow meadow]
              [--key value | --key=value ...]
              query name[:tag] [file ...]
```

The existing common command remains valid:

```text
forage --id=:0: --container=: --stemmer=porter :1: tf-idf
```

It means: apply default `tf-idf:none` to `:1:` results in every file containing
such a result, rank with `:` containers, and use `:0:` when an external ID is
needed.

A tagged, file-restricted form is:

```text
forage --id=:0: --container=: --stemmer=porter \
       :1: tf-idf:foo "collection*.tsv"
```

The first colon in `name:tag` separates the two components. Supplying no tag
means `none`. File arguments are expanded against Meadowlark's `/` inventory.
Supplying no files selects all files that contain the query.

More precisely, the application passes file arguments unchanged to
`forage_all(...)`; the library performs inventory expansion. Quoting a pattern
prevents the host shell from expanding it first. This is the same selector path
used by the Python wrapper. If any explicitly supplied selector matches no
meadow file, the operation reports an error rather than treating the resolved
empty set as "all files."

Arbitrary leading options continue to populate the parameter map and must be
passed unchanged to `Forager::check(...)`, every subclass factory, and the
primary metadata definition. The positional query is separate and must not be
silently accepted again through a conflicting option.

Supplying parameter options requests definition creation or exact validation.
Supplying none requests the existing-definition path: the primary record must
already exist, its query must match the positional query, and its stored
parameters are used. Programmatic callers retain a distinct way to supply an
explicitly empty parameter map.

## `TfIdfStats` Reader Changes

In `TfIdfStats::make(...)`, `recipe` is the TF-IDF tag. It is not the query.

For current metadata, Stats must select the unique primary record for:

```text
type=forager, name=tf-idf, tag=<recipe>, top-level query present
```

It must not select one of the potentially many file completion records sharing
the name and tag. It obtains:

- the content query from the primary record's top-level `query`;
- container, optional ID, stemmer, and tokenizer from `parameters`;
- numeric totals and term annotations from the index.

Completion metadata is not consulted during ranking. Aggregate annotations
surviving in the index determine the current statistics.

Preserve all established reader fallbacks:

- an empty recipe first probes legacy `@tf-idf:` metadata;
- otherwise the current default tag is `none`;
- old `parameters.contents`, then `parameters.gcl`, then `container` remain
  accepted when top-level `query` is absent;
- literal old JSON remains parseable;
- old annotation prefixes remain readable.

## Implementation Sequence

Work should proceed in small compile-checked steps after explicit coding
authorization:

1. Extend metadata serialization/parsing for common `filename`, top-level
   forager `query`, primary definitions, and parameter-free file completion
   records. Preserve old parsing.
2. Change new source-format metadata writers from `file` to `filename`; keep
   read-side acceptance of both spellings where needed.
3. Reshape `Forager` into a transaction-neutral interval worker: add the
   Warren-aware/query-aware validation path, keep factory dispatch, add the
   concrete existing-definition `make(warren, recipe, error)` path, remove
   metadata labeling and transaction ownership, and split worker finalization
   from `Warren::ready()`.
4. Update `TfIdfForager` and `NullForager` to the revised worker lifecycle.
5. Add primary-definition lookup, exact specification comparison, and narrow
   legacy TF-IDF refusal in Meadowlark's coordinator layer.
6. Add normalized `already_foraged(...)` using the file completion record.
7. Implement singular per-file `forage(...)` with explicit
   `(<< query normalized-filename)` scoping and coordinated atomic publication.
8. Implement `forage_all(...)` inventory-selector expansion, empty-vector
   discovery, preflight, skip/retry behavior, and the initial append-like
   JSON/TSV versus text/code scheduling policy.
9. Replace the old public interval/GCL forage overloads and update
   `apps/forage.cc` to the new positional query, `name[:tag]`, and optional
   selector interface. The application passes selector strings to the library
   without meadow-inventory expansion.
10. Update `TfIdfStats` to read only the new primary definition while retaining
    every old-reader path.
11. Add focused coverage and update the durable Meadowlark/exploration
    documentation to distinguish the new format from historical forms.
12. Compile-check affected library, application, and test targets. Do not run
    regression tests unless the user explicitly requests it.

## Focused Coverage

Tests should cover, without the agent running them:

- current definition and completion JSON shapes;
- compatibility parsing for literal and internally encoded old records;
- filename normalization and `filename`/`file` read compatibility;
- unique `(name, tag)` enforcement and exact query/parameter comparison;
- parameter-omitting existing-definition construction, including `recipe` as
  tag and failure when the primary definition is absent;
- invalid query and invalid implementation setup before definition commit;
- default `none` tag behavior;
- `already_foraged(...)` and batch skipping;
- explicit zero-result file completion;
- exact filename-feature scoping for TSV/JSON query intervals and text/code
  single-object behavior, including exclusion of cross-boundary results;
- empty selector vectors discovering all matching files, shared pattern
  expansion for command-line/Python callers, and nonempty unmatched selectors
  failing instead of selecting all files;
- atomic marker plus annotation publication and abort behavior;
- legacy TF-IDF refusal for writing;
- legacy TF-IDF ranking through `TfIdfStats`;
- current Stats selecting the primary definition rather than a file completion
  record;
- deletion-sensitive placement of TF-IDF worker aggregates inside their file.

## Non-Goals

This step does not:

- implement file deletion;
- migrate or rewrite old databases;
- support extending an old interval-oriented `(name, tag)` layer;
- add cross-process semantic locking for competing forage commands;
- remove the general `check(...)` pattern across Cottontail;
- change the indexed-regular-expression plan;
- design SQL, Mongo-style, join, or pipeline languages;
- optimize the final scheduling policy before the format is stable;
- change whether TF-IDF's stored DF annotations should remain.
