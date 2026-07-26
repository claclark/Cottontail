# Static Shards Plan

This is a deferred design for building a requested number of balanced,
standalone Hazel shards from Meadowlark-supported input files. It is not an
active implementation plan yet.

## Prerequisite

The current JSONL, TSV, text, and code append paths now provide the restartable
lifecycle needed by an initial implementation: durable source identity, typed
metadata, coordinated transaction publication, reliable detection of committed
inputs, and safe retry after interruption. `static-shards` depends on being
able to rerun Meadowlark with a complete assigned file list and have Meadowlark
skip committed files while appending only missing files. A future input type
must meet the same contract before `static-shards` accepts it.

The new app must reuse shared Meadowlark argument/append code or invoke the
existing `meadowlark` executable. It must not copy Meadowlark parsing and
ingestion logic into another app. The same principle applies to final
consolidation: call `Bigwig::consolidate(...)` directly or invoke
`apps/finish-merging` rather than copying its Fiver/Hazel lifecycle.

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
retains its input kind: TSV, JSONL, text, or code. Estimate the amount of input
in each file, including a reasonable policy for compressed files. Logical
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

## Serial Ingestion And Foreground Consolidation

Build one working meadow at a time because a single Meadowlark ingestion can
already consume the machine with worker threads. Each working meadow lives
beneath the output directory under a reserved name recorded in the manifest.

For a new working burrow, invoke Meadowlark with `--create`, its explicit
`--meadow` path, and the group's complete typed input list. For an existing
working burrow, omit `--create` and invoke Meadowlark with the same complete
input list. Meadowlark's preflight handles verification and recovery: committed
inputs are skipped and missing or interrupted inputs are appended.

Background Bigwig work may make progress while an ingestion process remains
open, but the builder must not depend on background workers reaching a
particular shard count. After all groups have been ingested, close their live
Warren views before invoking offline foreground consolidation.

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
4. Close the completed working burrow after ingestion. Its sanitized shard
   inventory is the durable input to foreground consolidation.

After replaying ingestion for every unpublished group, run the same foreground
consolidation, publication, and cleanup phases as a new build. Existing work
makes those phases finish immediately or continue from durable shard state.
The manifest records intent; Meadowlark transactions, Bigwig sanitization and
merge recovery, and final Hazel links record progress.

## Consolidation

After the last Meadowlark invocation, consolidate unpublished working burrows
one at a time with `Bigwig::consolidate(...)`. This is an offline operation, so
no live Warren for that burrow should remain in the builder.

Foreground consolidation sanitizes the directory, verifies contiguous shard
coverage, merges each maximal Fiver run once in memory, converts the merged
Fiver directly to Hazel without pickling it, performs the final Hazel merge,
and verifies that exactly one Hazel covers the complete sequence.

`apps/finish-merging` is the command-line caller of this operation and may be
invoked instead of duplicating it. Consolidations should remain serial because
the many-Hazel path is intended to use the machine's permitted thread budget.

## Finalization And Publication

Foreground consolidation leaves one standalone Hazel in the working burrow. A
Fiver is not a standalone Warren and must never be moved out or published
directly.

For each unpublished group:

1. Run or confirm successful foreground consolidation.
2. Verify that the resulting file is a standalone Hazel covering the group's
   complete sequence.
3. Hard-link that Hazel from the working burrow to its final
   `directory/shard.NN` name.
4. Only after the final link succeeds, remove the working burrow and its
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
