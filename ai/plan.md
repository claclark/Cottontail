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

## Active Step

Explicit foreground Bigwig consolidation is implemented and compile-checked.
The user's first regression run exposed two incorrect new test expectations;
both are corrected and the affected test targets compile. A regression rerun
and the full benchmark remain pending.

### Foreground Consolidation

- `Bigwig::make(...)` and `Bigwig::consolidate(...)` share burrow DNA parsing,
  component construction, parameter loading, and sanitized inventory creation.
  Activation and consolidation remain separate.
- `Bigwig::consolidate(...)` is an offline operation. It discards returned
  partial Hazel-merge recoveries, verifies one contiguous shard sequence,
  merges each maximal Fiver run once in memory, converts it directly to Hazel
  without pickling, and performs at most one final Hazel merge.
- Source shards remain in place until a replacement has been published.
  Sanitization then removes covered Fivers and Hazels. An interruption before
  publication leaves the sources intact; an interruption after publication is
  resolved by the next sanitization.
- `HazelMergeRecovery::discard(...)` removes its restart files idempotently.
  If that removal is interrupted, Hazel sanitization recognizes an incomplete
  recovery and removes the remainder.
- Verbose progress and phase timings are emitted only by
  `Bigwig::consolidate(...)`. `finish-merging --verbose` opts into them; the
  library operation is silent by default.
- `finish-merging` now invokes foreground consolidation directly instead of
  activating background workers and polling.

### Benchmark History

`build.sh` is the end-to-end benchmark. On the current MS MARCO workload, the
user reports this approximate wall-time shape:

- Meadowlark creation and TSV ingestion: 1 minute.
- TF-IDF foraging: 2 minutes.
- `finish-merging a.meadow`: nearly 30 minutes in background consolidation.

Before the foreground operation was added, `finish-merging` opened the burrow
to start Bigwig's normal background merge workers and polled every ten seconds
until only one Fiver or Hazel shard remained. The historical final-stage
measurements therefore cover the automatic conversion and consolidation policy
as a whole.

This host reports 28 logical CPUs. TSV ingestion and foraging each default to
`hardware_concurrency() + 1`, so each operation may publish 29 worker
transactions in addition to TSV source metadata. Earlier shards may consolidate
in the background while the forager is running, but the old `finish-merging`
path still saw a many-shard, multi-level merge workload rather than the older
three-large-shard microbenchmark.

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

### Foreground Consolidation Result

The user's first large run used normal background conversion during ingestion
and foraging. Foreground consolidation found two partial Hazel merges, five
completed Hazels, and 29 Fivers covering sequence 30-58. It completed in
21:11.05 elapsed at 99% CPU:

- Opening and sanitizing: 235 ms.
- Discarding two partial Hazel merges: 145 ms.
- Loading 29 Fivers: 31,871 ms.
- Merging 29 Fivers once in memory: 59,545 ms.
- Converting the merged Fiver to Hazel: 505,442 ms.
- Merging the resulting six Hazels: 665,872 ms.
- Final sanitization: 886 ms.

The wide dynamic Fiver work took only about 91 seconds. Fiver-to-Hazel
conversion and the final Hazel merge consumed about 92% of the foreground
time, with the Hazel merge the largest single phase. Peak resident memory was
reported as 16,848,612 KiB.

### Likely Shared Cost

The index paths converge on `SimplePosting`:

- Fiver merging uses `SimplePostingFactory::posting_from_merge(...)`.
- Fiver-to-Hazel conversion serializes each list with
  `SimplePosting::write(...)`.
- Hazel merging decodes source lists into `SimplePosting`, merges them, and
  serializes them again with `SimplePosting::write(...)`.

Meadowlark configures `zlib` for feature values. Before the current experiment,
every value-bearing posting list allocated a value-compression buffer with
64 KiB of extra capacity and initialized a fresh `Z_BEST_COMPRESSION` stream.
Hazel decoding performed the matching fresh inflate initialization per list.
The 29-Fiver suffix was produced by foraging and is dominated by value-bearing
annotations, yet its Fiver-to-Hazel conversion took 505 seconds. This makes
per-list value compression and allocation the strongest common hypothesis.

The ordered fast path in `posting_from_merge(...)` also copies every element
through `get`/`push` despite the existing bulk `SimplePosting::append(...)`
operation. This can affect both Fiver and Hazel merges, although the measured
wide Fiver merge was much smaller than serialization.

Hazel merging has an additional format-specific cost: it flushes the posting
checkpoint after every non-singleton list and the directory checkpoint after
every feature. With millions of features, this should be measured separately
from the shared `SimplePosting` path.

### Zlib Experiment Result

`ZlibCompressor` now keeps one lazily initialized deflater and inflater per
thread. Each operation uses `deflateReset(...)` or `inflateReset(...)`, which
preserves independent zlib blobs while retaining internal allocation. The
shared compressor object remains stateless and thread-safe. Output capacity now
uses `compressBound(...)` rather than adding a fixed 64 KiB.

Focused coverage compares repeated output byte-for-byte with fresh
`compress2(..., Z_BEST_COMPRESSION)` output and exercises one shared compressor
from eight threads. The library, aggregate test binary, and all three
`build.sh` applications compile.

The user's large benchmark moved foreground consolidation from 1,269,902 ms to
1,259,340 ms, a 10,562 ms (0.83%) improvement. Fiver conversion moved from
505,442 ms to 501,577 ms (0.76%), and Hazel merging moved from 665,872 ms to
661,538 ms (0.65%). Ingestion and foraging were effectively unchanged. This is
small enough to overlap normal run variation, so repeated zlib initialization
is not the main bottleneck.

Keep the zlib change as a small, format-compatible cleanup: it removes needless
per-call zlib allocation and the fixed 64 KiB output allowance without adding
substantial complexity. Do not treat it as the performance fix.

### Planned SimplePosting Cleanup

Keep this work small and preserve the current compact representation:

- Empty `qostings_` means every annotation is a point (`q == p`).
- Empty `fostings_` means every feature value is zero.
- Expanded vectors may still contain points or zero values alongside intervals
  or nonzero values.

Before optimizing, add table-driven coverage for the four source shapes:
point/no-value, point/value, interval/no-value, and interval/value. Exercise
all ordered pairs through both appendable and interleaved merges, verify the
result triples and invariants, serialize/deserialize the result, and inspect
the serialized record to ensure qostings and fostings remain absent whenever
the merged data permits it. Retain focused coverage for duplicate resolution
and exclusion filtering.

Then make three local `SimplePosting` changes:

1. In the ordered `posting_from_merge(...)` fast path, use the existing bulk
   `append(...)` operation instead of copying every entry through `get(...)`
   and `push(...)`. The existing append logic already expands missing
   qostings with `p` and missing fostings with zero only when another input
   requires the expanded representation.
2. In `posting_from_compressed_blob(...)` and `posting_from_file(...)`, resize
   only the vectors present in the serialized record and decompress directly
   into their storage. Remove the temporary decoded array and per-element
   `push_back(...)` copies.
3. Give `SimplePosting::write(...)` a small thread-local scratch object holding
   posting, qosting, and fvalue byte buffers. Resize qosting and fvalue buffers
   only when those vectors are present. This removes per-list heap allocation
   without changing the public API or file format.

Compile after each change, then let the user run the regression suite and
`build.sh` once for the combined `SimplePosting` cleanup.

After that measurement, make a separate small Hazel-driver cleanup:

- Reuse one source-posting vector across features rather than allocating it in
  every loop iteration.
- Use the already maintained sequential directory positions to identify and
  load sources for the current feature instead of binary-searching every Hazel
  directory again.
- Reserve known directory/checkpoint capacity where an inexpensive upper bound
  is already available.

Do not yet change the posting format, singleton encoding, priority-queue merge,
exclusion semantics, or checkpoint flush/recovery policy. Those require
separate evidence and discussion.
