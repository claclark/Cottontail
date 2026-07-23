# Current Plan

## Working Rule

No coding without discussion and explicit confirmation from the user.

Work on one agreed step at a time. Approval for one step does not authorize
starting a later step. Entries in `ai/improvements.md` are possibilities, not
active work, until the user promotes one into this plan.

If an agreed change appears to require a broader abstraction or materially
different design, stop and discuss it. Proposals for cleanup and refactoring
are welcome, but they also require discussion before implementation.

## Current Checkpoint

The Meadowlark JSONL, TSV, text, and code ingestion work is complete. The
current worktree has passed compile checks, and the user is running the
regression tests. Durable database and metadata conventions are recorded in
`ai/meadowlark.md`; the model-facing database bootstrap guide is
`ai/exploring-meadowlark.md`.

Meadowlark is no longer the active design area. Preserve its discovery,
metadata, provenance, transaction, and restart contracts, but do not continue
with another append surface unless the user explicitly returns to it.

## Next Step

The parameter-driven Fiver-only consolidation experiment is implemented and
compile-checked. Runtime tests and the benchmark have not been run.

### Benchmark

`build.sh` is the end-to-end benchmark. On the current MS MARCO workload, the
user reports this approximate wall-time shape:

- Meadowlark creation and TSV ingestion: 1 minute.
- TF-IDF foraging: 2 minutes.
- `finish-merging a.meadow`: nearly 30 minutes, dominated by slow Hazel/Hazel
  merges.

`finish-merging` does not implement a separate foreground merge. Opening the
burrow starts Bigwig's normal background merge workers, and the application
polls every ten seconds until only one Fiver or Hazel shard remains. The final
stage therefore measures the automatic conversion and consolidation policy as
a whole.

This host reports 28 logical CPUs. TSV ingestion and foraging each default to
`hardware_concurrency() + 1`, so each operation may publish 29 worker
transactions in addition to TSV source metadata. Earlier shards may consolidate
in the background while the forager is running, but `finish-merging` still sees
a many-shard, multi-level merge workload rather than the older three-large-shard
microbenchmark.

### Implemented Experiment Support

- `merge` and `convert` are live Fluffle parameters rather than cached Boolean
  fields. Lock-held policy checks default both parameters to enabled when
  absent.
- `merge:no` prevents new merge-action selection. A worker already performing
  an action finishes it, then retires when it next checks policy.
- `convert:no` disables all three Fiver-to-Hazel selection paths and removes
  the normal 256 MiB input cap from Fiver/Fiver pair selection. A Fiver-only
  database can therefore consolidate to one large Fiver and retain an
  in-memory index after activation.
- `Bigwig::merge(on, convert)` remains the active trigger. `merge()` enables
  merging and conversion, `merge(false)` disables merging without changing
  conversion policy, and `merge(true, false)` enables Fiver-only consolidation
  and calls `try_merge()`.
- `meadowlark --create` accepts checked `parameter:value` assignments before
  the first input flag and applies them with `set_parameter(...)` before
  ingestion, updating both live Fluffle parameters and DNA.

### Experiment Results

The user ran the equivalent of `build.sh` with `convert:no`:

- Meadow creation and TSV ingestion: 1:02.82 elapsed.
- TF-IDF forage: 3:29.02 elapsed.
- `finish-merging`: 38:20.55 elapsed, averaging 224% CPU.

The final meadow contains one 3,634,617,502-byte Fiver and no Hazels. The 59
transaction-sequence shards therefore converged as intended, but the merge was
not faster than the Hazel-producing run. `finish-merging` reported 39,504,512
filesystem output blocks, approximately 20.2 GB if interpreted as Linux
512-byte `ru_oublock` units: about 5.6 times the final Fiver size. This output
count is evidence that the data passes through several merge levels; it is not
evidence that storage bandwidth is the bottleneck.

This rules out Hazel conversion as the primary cause of the long final merge.
The timing is predominantly user CPU (5,068.43 seconds user versus 93.51
seconds system), with no swaps or major page faults, so CPU transformation and
memory traffic are more plausible primary costs than I/O. The current pairwise
merge tree repeatedly reconstructs and pickles the text and postings at each
level. This includes copying raw text, rebuilding maps and posting vectors,
compressing the entire text with zlib at its best-compression setting, and
recompressing every posting. Early pairs can run concurrently, while the upper
levels leave only one or two large jobs and account for a mostly serial tail.
The Fiver-only mode remains independently useful because it retains an
in-memory index, but it is not a lower bound on final consolidation time.

The simple-Warren path is an important comparison. `SimpleBuilder::build_index`
opens all posting runs together, processes them in one feature-ordered pass,
and bulk-appends ordered posting vectors before writing each feature once.
Simple text is compressed incrementally in 1 MiB chunks and is not rebuilt
during the index merge. Bigwig's Fiver path instead performs repeated binary
whole-shard transformations, and its generic ordered-posting merge copies
entries one at a time before the result is compressed again. This comparison
strengthens the CPU/work-amplification diagnosis without identifying which of
text compression, posting reconstruction, posting compression, or allocation
is dominant.

### Next Discussion

The next investigation should first separate time spent in Fiver text assembly,
posting reconstruction, text compression, posting compression/writing, and
publication. After that, merge scheduling and fan-in remain strong design
candidates. Both merge implementations already accept vectors of inputs, so
wider groups or an explicit wide final merge may reduce the number of times the
corpus is copied and recompressed. The memory and concurrency tradeoffs need to
be considered separately for normal incremental maintenance and the explicit
`finish-merging` operation before any code is changed.

The checkpoint-aware unique-source Hazel raw-copy path in
`ai/improvements.md` may still be worthwhile, but it is now a secondary
optimization; historically it improved a different three-large-Hazel workload
by only about 2.75%.
