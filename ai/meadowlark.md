# Meadowlark Plan

Status date: 2026-06-27.

This note records the intended shape of Meadowlark ingestion after the
`append_jsonl(...)` restart and Scribe transaction work. It is a planning note,
not an implementation requirement for unrelated code.

## Intended Model

Meadowlark should expose a small set of append operations that all follow the
same lifecycle:

1. Normalize the source identity.
2. Start the original Warren at the top of the append operation.
3. Use that started Warren as the read snapshot for restart checks and clone
   creation.
4. Check whether the source identity is already present in the meadow.
5. If already present, optionally report a skip and return success.
6. Record the source identity once, producing the feature used to annotate the
   appended records.
7. Create worker clones from the started Warren.
8. Write through the narrowest appropriate abstraction.
9. Ready every worker transaction.
10. Commit workers through the highest relevant batch helper.
11. Finalize any Scribes used by the operation.
12. End every clone and then end the original Warren on every return path.

The important layering rule is that callers should stay at the abstraction
level they chose. If an append path writes through `Scribe`, it should manage
transactions through `Scribe` and commit through `Scribe::commit_all(...)`.
`Scribe::commit_all(...)` can discover Warren-backed scribes, and
`Warren::commit_all(...)` can discover Bigwig warrens. Meadowlark should not
know whether it is getting Bigwigs over one working directory.

## Source Identity

Restartability needs a durable identity string for each append operation.
File-based appends can use the normalized filename. Non-file appends should
take an explicit source label or append id; without one, duplicate detection is
not well-defined.

The current identity marker is a piece of appended text annotated with `/`, and
the per-record annotations use the featurized source identity. That is workable
for file paths and should be generalized before adding non-file sources. The
name `append_path(...)` is now narrower than the role it plays.

The existing JSONL restart check looks up `(>> / "<source>")`, translates all
matching intervals, and accepts leading whitespace plus trailing whitespace
before any later junk. Future source types should reuse that behavior instead
of each growing a separate duplicate check.

## Planned Append Surfaces

### JSONL Files

`append_jsonl(warren, filename, ...)` is the current prototype for the common
pattern.

Current useful behavior:

- Starts the original Warren at the top.
- Checks for an existing source identity before opening the input.
- Streams plain or gzipped input with `maybe_zipped(...)`.
- Writes records through per-clone `Scribe`s.
- Readies through `Scribe`.
- Commits through `Scribe::commit_all(...)`.
- Finalizes all Scribes as the last formal Scribe step.

Near-term cleanup should focus on extracting the reusable pieces without
changing behavior.

### JSONL From Strings

Add an append operation for an array/vector of JSON strings. It should share
the JSON line processing with file JSONL and differ only in the input source.

Required API shape:

- The caller should provide a source identity string.
- The function should be able to skip if that identity is already present.
- The function should use the same Scribe worker lifecycle as file JSONL.

Open question: whether the API should accept already-split strings only, or an
input object that can also represent streaming sources. Start with the simple
vector form unless the implementation becomes awkward.

### TSV Files

`append_tsv(...)` should move onto the common append lifecycle.

Desired behavior:

- Start the original Warren at the top and end it on every return path.
- Check the normalized filename before reading the whole file.
- Use the same source identity helper as JSONL.
- Use the same worker lifecycle pattern.
- Prefer `Scribe::commit_all(...)` if TSV writes through Scribes; otherwise
  use `Warren::commit_all(...)` only when the code is intentionally at the
  Warren layer.
- Preserve the current record/field annotation semantics unless there is a
  separate design discussion.

Current inconsistency: TSV still uses direct Warren transactions, direct clone
starts, direct commits, no restart skip check, and no verbose skip/append
reporting.

### Raw Text Files

Add append support for raw text files using the same source identity and
restart pattern.

The first version can treat each file as one record:

- Append the file contents.
- Annotate the whole interval with the source identity feature.
- Optionally add small structural annotations such as `file:`, `filename:`,
  or `content:` if that matches the intended Meadowlark query surface.

Do not invent special transaction mechanics for raw text. It should be another
source adapter feeding the shared append lifecycle.

### Code

Code ingestion should be another source adapter, not a separate commit path.

Likely first shape:

- Append each source file as raw text.
- Annotate the full file interval with the source identity feature.
- Add language, repository, path, and maybe line-span annotations as metadata.
- Leave parser-aware symbols, definitions, imports, and references for a later
  pass once the storage lifecycle is boring.

Code append must use the same restart and batch-commit pattern as text/jsonl.

## Current Inconsistencies To Resolve

- `append_jsonl(...)` is now the only append path with restart detection.
- `append_jsonl(...)` has `verbose = true`; `append_tsv(...)` does not.
- `append_jsonl(...)` uses the Scribe lifecycle; `append_tsv(...)` writes and
  commits through Warrens directly.
- `append_jsonl(...)` relies on clones of a started Warren being started;
  `append_tsv(...)` still explicitly starts clones.
- `append_jsonl(...)` uses `Scribe::commit_all(...)`; `append_tsv(...)` commits
  each clone individually.
- `append_path(...)` now records a general source identity but is still named
  and typed as a path helper.
- `thread_count(...)` policy differs: JSONL uses `hardware_concurrency() + 1`
  directly, while TSV uses the local helper and forces small inputs to one
  thread.
- `forage(...)` still uses its own direct Forager/Warren transaction pattern.
  That may be fine because it is not an append source, but it should be checked
  later for commit batching and error propagation consistency.

## Suggested Order

1. Extract source identity helpers around normalization, duplicate detection,
   and source marker insertion.
2. Keep JSONL behavior stable while making the helper boundaries clearer.
3. Move TSV onto the same top-level start/end, restart check, verbose status,
   and batch commit shape.
4. Add JSONL-from-strings using the JSONL worker body and explicit source
   identity.
5. Add raw text file append.
6. Add code append as raw text plus metadata first.
7. Revisit foraging only after append operations share the same pattern.

