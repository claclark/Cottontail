# Static Shards Plan

This is a deferred design for building a requested number of balanced,
standalone Hazel shards from Meadowlark-supported input files. It is not an
active implementation plan yet.

## Prerequisite

The current JSONL and TSV append paths now provide the restartable lifecycle
needed by an initial implementation: durable source identity, typed metadata,
coordinated transaction publication, reliable detection of committed inputs,
and safe retry after interruption. `static-shards` depends on being able to
rerun Meadowlark with a complete assigned file list and have Meadowlark skip
committed files while appending only missing files. A future input type must
meet the same contract before `static-shards` accepts it.

The new app must reuse shared Meadowlark argument/append code or invoke the
existing `meadowlark` executable. It must not copy Meadowlark parsing and
ingestion logic into another app. The same principle applies to final Fiver to
Hazel conversion: share the relevant implementation or invoke the existing
tooling demonstrated by `apps/fiver2hazel.cc`.

## Command Shape

The intended initial-build shape is:

```text
static-shards [--directory directory] shard-count [meadowlark arguments...]
```

The default output directory is `shards`, producing final files such as:

```text
shards/shard.00
shards/shard.01
```

The directory is selectable on the command line. Final shard numbers use a
width derived from the capped shard count, with a minimum width of two.

Restart uses only the existing output directory:

```text
static-shards --restart ClimbMix.shards
```

The restart command reads the published plan. It does not repeat Meadowlark
arguments, remeasure files, or repartition inputs.

## Partitioning

Parse the forwarded Meadowlark arguments into typed input records so each file
retains its input kind, such as TSV or JSONL. Estimate the amount of input in
each file, including a reasonable policy for compressed files. Logical
uncompressed size is a better estimate of indexing work when it can be obtained
cheaply and reliably; stored size is an acceptable best-effort fallback. Record
the chosen measurement in the plan so restart never changes the partition.

Cap the actual shard count at the number of input files. Every output shard is
nonempty, so six input files produce at most six shards even if 1,000 are
requested. Files are indivisible but may be reordered.

Use a greedy longest-processing-time assignment:

1. Sort input files from largest estimated size to smallest.
2. Maintain the requested nonempty output groups ordered by assigned size.
3. Assign each file to the currently smallest group.

Balanced indivisible partitioning is NP-hard; this greedy algorithm is adequate
for the intended best-effort tool.

## Durable Plan

The output directory is a durable build-job directory, not disposable scratch
space. Before creating any working burrow:

1. Create the output directory.
2. Compute the capped shard count and greedy assignment.
3. Write the complete plan to a temporary file inside the output directory.
4. Atomically link the temporary file to `README.md`.

`README.md` is both the immutable restart manifest and a human explanation of
the directory. Its exact format remains to be designed, but it must be safely
machine-readable and clearly record:

- the plan/version format;
- the original normalized Meadowlark arguments;
- each input's type, path, and measured size;
- the capped shard count and complete file assignment;
- each working-burrow path;
- each final `shard.NN` path; and
- the sizing policy used for compressed inputs.

It must also tell a human exactly how to restart the build, including the
concrete command for this output directory, for example:

```text
static-shards --restart ClimbMix.shards
```

If the process dies before the link, no plan was published and the temporary
file can be discarded. Once `README.md` exists, it is authoritative and must
not be silently replaced or recomputed.

## Serial Ingestion And Concurrent Consolidation

Build one working meadow at a time because a single Meadowlark ingestion can
already consume the machine with worker threads. Each working meadow lives
beneath the output directory under a reserved name recorded in the manifest.

For a new working burrow, invoke Meadowlark with `--create`, its explicit
`--meadow` path, and the group's complete typed input list. For an existing
working burrow, omit `--create` and invoke Meadowlark with the same complete
input list. Meadowlark's preflight handles verification and recovery: committed
inputs are skipped and missing or interrupted inputs are appended.

After each Meadowlark invocation succeeds, open that working meadow as a Warren
and retain it. Its Bigwig background workers continue consolidation while the
next group is ingested. Although ingestion is serial, consolidation of completed
groups may therefore overlap later ingestion.

Every committed intermediate Bigwig is a valid queryable collection. A
half-constructed shard is incomplete only with respect to its manifest; it is
not an invalid index.

## Restart

`--restart` reads `README.md` and runs the published plan again from the
beginning. Restart is not a separate recovery algorithm and does not require a
mutable progress database. The ordinary steps must be idempotent enough that
replaying the plan is recovery.

For each group in plan order:

1. If its final standalone `shard.NN` already exists and validates, publication
   is complete and the group returns immediately apart from any leftover
   cleanup.
2. Otherwise, create its working burrow if absent or reopen it if present.
3. Invoke Meadowlark with the group's complete assigned input list. Meadowlark
   skips committed files and appends only missing or interrupted files, so a
   completed group returns quickly and an incomplete group resumes normally.
4. Open and retain the completed working burrow as a Warren. Bigwig activation
   resumes consolidation from its durable shard state.

After replaying ingestion for every unpublished group, run the same wait,
conversion, publication, and cleanup phases as a new build. Existing work makes
those phases finish immediately or continue from their durable state. The
manifest records intent; Meadowlark transactions, Bigwig restartability, and
final Hazel links record progress.

## Consolidation

After the last Meadowlark invocation, retain all unfinished working Warrens and
poll their directories, approximately every 10 seconds. Count entries matching
`fiver.*` or `hazel.*`. A working burrow is ready for finalization when exactly
one such shard remains. Continue until every unpublished group reaches that
state.

This is the same completion condition used by `apps/finish-merging`. Reuse that
logic where practical rather than introducing another subtly different notion
of consolidation.

## Finalization And Publication

Stop retaining the working Warrens before manipulating their final files. A
Fiver is not a standalone Warren and must never be moved out or published
directly.

For each unpublished group:

1. If its one remaining internal shard is a Fiver, convert it to a Hazel inside
   the original working burrow. Reconstruct the featurizer, tokenizer,
   compressors, and parameters from that burrow's DNA as `fiver2hazel` does.
2. If the remaining shard is already a Hazel, use it directly.
3. Verify that the resulting file is a standalone Hazel covering the group's
   complete sequence.
4. Hard-link that Hazel from the working burrow to its final
   `directory/shard.NN` name.
5. Only after the final link succeeds, remove the working burrow and its
   intermediate files.

The final hard link is the publication commit point. A crash before it leaves
the restartable working burrow intact. A crash after it leaves a complete
standalone shard plus possibly redundant working files; restart treats the final
shard as complete and finishes cleanup. Never remove a working burrow before
its final standalone Hazel has been published successfully.

## Result

A successful build leaves the immutable `README.md` plan and the requested
number of balanced standalone Hazel files:

```text
shards/README.md
shards/shard.00
shards/shard.01
...
```

No dynamic working burrows or intermediate merge files remain after cleanup.
