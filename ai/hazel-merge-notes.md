# Resumable Hazel Merge Notes

This note records the implemented direction for an interruptible, retryable
Hazel-to-Hazel merge. The merge is subtle enough to stand as its own design and
status note. It is not a Fluffle policy document.

This note deliberately stays below Fluffle policy. It does not decide when a
merge should run, which shard range should be selected, how merged shards are
installed into the live view, or how Fivers are retained. Those choices remain
above this file-level merge primitive.

## Current Status

The first activated-Hazel merge path has been added in `src/hazel.cc` and
declared in `src/hazel.h`:

```
Hazel::merge(hazels, dst, parameters, error)
```

where the concrete declaration is:

```
static std::shared_ptr<Hazel> merge(
    const std::vector<std::shared_ptr<Hazel>> &hazels,
    const std::string &dst,
    std::shared_ptr<std::map<std::string, std::string>> parameters,
    std::string *error = nullptr);
```

On success, the activated-Hazel overload returns the activated but unstarted
output Hazel for `dst`.

The old `Hazel::merge(working, hazel_names, parameters, error)` surface is
still present for callers, but its old file-parsing merge implementation has
been removed. It is now a thin compatibility adapter: parse the old Hazel shard
names, activate each Hazel file with `Warren::make(...)`, downcast to `Hazel`,
verify filename/DNA sequence agreement when sequence metadata is present, and
call the activated-Hazel overload with a null parameter pointer, reporting
success when it returns a non-null output Hazel.

As of 2026-06-13, repeated manual-interruption smoke checks on `a.meadow`
completed successfully. A final Hazel produced after three interrupted
`fiver2hazel --merge a.meadow` runs matched the known saved output size and
ranked with the expected MARCO dev-small profile. Details live in
`ai/hazel-progress.md`.

## Core Goal

Build a Hazel merge operation that can be safely retried after interruption.

The important behavior is now:

- if the final destination already exists, report success after deleting
  associated intermediates;
- if `mrg.<dst-name>` exists without the final destination, discard it;
- resume the long idx posting merge from `pst.<dst-name>` and
  `dct.<dst-name>`;
- assemble a fresh `mrg.<dst-name>` using activated Hazel objects;
- publish the completed Hazel with a final hard link to `dst`;
- clean up associated intermediates after success.

Only the idx side has durable checkpoint files. The txt side is short and
deterministic: it directly copies already-compressed text chunks into the final
assembly stream without decompressing or recompressing them.

## Brave Assumptions

These assumptions are intentional for this first implementation.

- One process owns directory mutation for the burrow. In practice, one Fluffle
  is managing the directory. There is no directory-level lock yet; later work
  should make this invariant real.
- The caller chooses `dst` as the unique final name for this merge attempt.
  The associated `mrg.`, `pst.`, and `dct.` sidecars belong to this merge and
  do not collide with unrelated work.
- Existing idx checkpoint files with this prefix are trusted to be
  continuations of the same requested merge. There is no separate manifest
  proving that `pst.<dst-name>` and `dct.<dst-name>` match the input Hazel
  list.
- The merge does not require explicit flush or fsync checkpoints. It streams
  checkpoint bytes and recovers by trusting only complete directory records
  whose referenced posting bytes are actually present.
- The common case is expected to be boring: inputs are the intended activated
  Hazels, checks succeed, checkpoints if present are sane, and the merge
  continues.

If these assumptions are false, behavior may be limited to ordinary validation
failure. The first version is not trying to defend against hostile files,
multiple writers, or a caller reusing a prefix for a different merge.

## Main Shape

The active merge shape is object-assisted and file-published:

- `hazels` is the caller-selected list of activated Hazel shards;
- `dst` is the final output filename and the unique prefix for intermediates;
- `parameters` is the optional shared Fluffle parameter map for the merged
  Hazel DNA;
- `Hazel::merge` owns whole-file assembly, final publication, and prefix
  cleanup;
- `HazelIdx::merge` owns resumable idx construction and appends a complete idx
  blob to the open temp stream;
- `HazelTxt::merge` appends a complete txt blob to the open temp stream by
  copying compressed chunks.
- on success, the top-level activated merge opens and returns the output Hazel
  without calling `start()`.

The helpers require at least two inputs. Calling either helper with zero or one
input is treated as a caller error.

## Hazel Object Contract

The merge treats activated Hazel objects as read-only handles.

- Input Hazels may be serving other reader threads while the merge runs.
- The merge does not mutate input Hazel objects, call `start()` or `end()` on
  them, steal their `idx_` or `txt_`, or disturb query cursor state.
- `Hazel::merge` downcasts the activated components to concrete `HazelIdx` and
  `HazelTxt`. If either cast fails, the merge returns an error.
- `HazelIdx::merge` reads immutable input posting bytes through the existing
  activated `HazelFile` handle.
- `HazelTxt::merge` reads immutable compressed text chunks through the existing
  activated `ReadGate`.

This is intentionally Hazel-specific. We are not adding generic `Idx` or `Txt`
merge APIs for this primitive.

## DNA, Parameters, And Sequences

Input Hazel DNA is checked lightly.

- Inputs must have compatible DNA after ignoring the top-level `parameters`
  package and `hazel.sequence_start` / `hazel.sequence_end`.
- `txt` `chunk_size` is not ignored by the activated-Hazel compatibility check;
  in practice the merged Hazels come from the same family and should have the
  same DNA.
- `HazelTxt::merge` also performs a belt-and-suspenders equality check on the
  activated `target_chunk_size_` values before writing the txt blob header.

Output `parameters` handling follows Fluffle ownership in the activated
overload:

- if the shared `parameters` pointer passed to `Hazel::merge` is non-null, the
  output DNA writes `parameters: freeze(*parameters)`;
- if the pointer is null, the output DNA inherits the `parameters` package from
  the last input Hazel, or omits it if the last input has none.

The compatibility Working/name adapter deliberately passes `nullptr`, matching
the current `fiver2hazel` behavior where converted input Hazels already carry
the desired parameter package.

Sequence metadata is optional but meaningful:

- if input Hazels have `hazel.sequence_start` and `hazel.sequence_end`, all
  inputs must have them;
- the input ranges must be contiguous in the caller-supplied order;
- the output DNA writes the first input's `sequence_start` and the last input's
  `sequence_end`;
- if sequence metadata is absent, the merge does not invent it.

Sequence selection, candidate selection, and live-view replacement remain
Fluffle policy.

## File State Protocol

Known files:

- `dst`: final published Hazel;
- `mrg.<dst-name>`: final assembly temp;
- `pst.<dst-name>`: growing merged idx posting payload bytes;
- `dct.<dst-name>`: growing merged idx directory records.

Startup behavior:

1. If `dst` exists, the merge is already complete. Delete associated
   intermediates and report success. Transitional cleanup also removes
   old-style `dst.*` sidecars.
2. If `mrg.<dst-name>` exists but `dst` does not, delete `mrg.<dst-name>`. A
   temp assembly file is never trusted.
3. Validate activated Hazel inputs and compute merged DNA.
4. Create a fresh `mrg.<dst-name>`, write the file header, merged DNA, and
   placeholder blob dictionary.
5. Call `HazelIdx::merge(...)` to repair/resume `pst.<dst-name>` /
   `dct.<dst-name>` and append a complete idx blob.
6. Call `HazelTxt::merge(...)` to append a complete txt blob by copying
   compressed chunks.
7. Patch the top-level blob dictionary, close `mrg.<dst-name>`, link it to
   `dst`, and delete associated intermediates.

If any step before linking fails, remove `mrg.<dst-name>` and return failure.
Do not delete `pst.<dst-name>` or `dct.<dst-name>` merely because assembly
failed; those files are the resumable idx work product. A later retry should
discard `mrg.<dst-name>`, repair the idx checkpoint files, and assemble again.

## Dct/Pst Checkpointing

`dct.<dst-name>` is the real checkpoint for the long idx merge.

Each completed output posting is committed in this order:

1. append its encoded posting bytes to `pst.<dst-name>`, unless the output
   directory entry is an inline singleton;
2. append one complete Hazel posting directory record to `dct.<dst-name>`.

The on-disk directory record is three serialized `addr` values:

```
addr feature
addr end
addr count_or_p
```

`end` retains the normal Hazel meaning: it is a cumulative offset relative to
the start of the final idx blob. The first posting starts immediately after
the idx blob header:

```
hazel_idx_magic.size() + 3 * sizeof(addr)
```

On resume, `HazelIdx::merge`:

1. truncates `dct.<dst-name>` to a multiple of `3 * sizeof(addr)`;
2. reads the complete directory records, if any;
3. inspects the actual size of `pst.<dst-name>`;
4. computes `covered_end = idx_header_length + pst.<dst-name>.size()`;
5. keeps the largest prefix of `dct.<dst-name>` whose records are sane, strictly
   increasing by feature, non-decreasing by `end`, and covered by
   `covered_end`;
6. truncates `dct.<dst-name>` to that surviving prefix;
7. truncates `pst.<dst-name>` to the surviving last
   `end - idx_header_length`, or to zero if no directory records survive;
8. resumes at the first feature greater than the last committed feature.

If there is no complete directory record, `pst.<dst-name>` is treated as empty.

If interruption happens after posting bytes are appended but before the
directory record is fully appended, the posting bytes are uncommitted and will
be trimmed. If interruption leaves a directory record whose posting bytes are
not fully present in `pst.<dst-name>`, that directory record and any later
records are trimmed. The recovery/truncation process is idempotent and
restartable.

## Idx Merge Ownership

The resumable idx merge lives in a static internal helper on `HazelIdx`:

```
HazelIdx::merge(idxs, text_lengths, text_chunk_feature, pst, dct, out, error)
```

Current ownership:

- `HazelIdx::merge` receives the explicit `pst.<dst-name>` and
  `dct.<dst-name>` checkpoint filenames;
- it performs dct/pst cleanup, truncation, validation, and resume;
- it merges idx postings and commits each output feature through dct/pst;
- after the checkpoint pair is complete, it writes the Hazel idx blob header
  and copies the checkpoint files into the open output stream.

The `null_feature` erasure posting does not need a separate control file.
Because it is feature zero, it is part of the processing invariant. Before the
ordinary feature sweep begins, `HazelIdx::merge` ensures that `null_feature`
has already been processed and recorded if any input contains erasures. The
resulting merged erasure posting is then the `exclude` list passed to
`SimplePostingFactory::posting_from_merge(postings, exclude)` for ordinary
features.

The `text_chunk_tag` posting also does not need a separate control file.
It is synthesized when the feature sweep reaches it. `HazelIdx::merge` uses
the raw text length of each input Hazel, gathered from `HazelTxt`, so it can
adjust text chunk byte anchors in the merged posting.

The current implementation reads and merges posting records directly through
`SimplePostingFactory`. Restoring a raw-copy fast path for unique features can
be considered later if needed.

## Txt Merge Ownership

The txt side lives in a static internal helper on `HazelTxt`:

```
HazelTxt::merge(txts, out, error)
```

It is intentionally boring:

- require at least two `HazelTxt` inputs;
- lightly check all activated `target_chunk_size_` values match;
- write a Hazel txt blob header with placeholders;
- copy compressed text chunks from inputs to the output stream;
- do not decompress or recompress text;
- build a new text directory with cumulative raw and compressed byte ends;
- patch the txt blob header.

`HazelTxt` now retains the activated `raw_text_length_` and
`target_chunk_size_`. `Hazel::merge` uses `raw_text_length()` to build the
`text_lengths` vector for `HazelIdx::merge`.

## Whole-File Assembly

`Hazel::merge` owns the final file.

Implemented high-level flow:

1. handle idempotent `dst` success and cleanup;
2. remove any stale `mrg.<dst-name>`;
3. validate activated input Hazels, downcast idx/txt components, and compute
   merged DNA metadata;
4. create `mrg.<dst-name>`;
5. write the Cottontail file header, merged DNA, and placeholder blob
   dictionary;
6. append the idx blob through `HazelIdx::merge(...)`;
7. append the txt blob through `HazelTxt::merge(...)`;
8. patch the top-level blob dictionary;
9. close `mrg.<dst-name>`;
10. link `mrg.<dst-name>` to `dst`;
11. delete associated intermediates.

The Hazel envelope does not require blobs to appear in a fixed physical order.
The current activated-Hazel merge writes idx first and txt second, and the
top-level blob dictionary records the resulting offsets and lengths.

## Remaining Cleanup

The old file-parsing Hazel merge has been removed from `src/hazel.cc`.
`HazelMergeInput`, the legacy idx/txt merge helpers, and the old
`posting_for_feature(...)` / `text_chunk_posting(...)` /
`posting_factory_from_idx_recipe(...)` support code are gone.

The old Working/name API remains only as a compatibility adapter for existing
callers such as `apps/fiver2hazel.cc` and Hazel regression coverage. Future
Fluffle integration can call the activated-Hazel overload directly once it has
selected and activated candidate Hazels.

Compile/build verification is sufficient unless the user explicitly asks for
runtime tests, ranking runs, evals, or benchmarks.
