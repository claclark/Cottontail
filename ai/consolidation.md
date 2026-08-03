# Bigwig Consolidation Checkpoint

This is the durable record of the completed foreground-consolidation,
restartable parallel Hazel-merge, performance, recovery, and verification work.
It was moved out of `ai/plan.md` after completion so the current plan can remain
short and forward-looking.

## Foreground Consolidation

Explicit foreground Bigwig consolidation is implemented and tested. The
user's initial regression failures exposed incorrect test expectations and
were corrected; a later full `make testing` run passed. The large foreground
benchmarks and subsequent performance work are recorded below.

### Implementation

- `Bigwig::make(...)` and `Bigwig::consolidate(...)` share burrow DNA parsing,
  component construction, parameter loading, and sanitized inventory creation.
  Activation and consolidation remain separate.
- `Bigwig::consolidate(...)` is an offline operation. Gaps caused by aborted
  commits are allowed. It greedily partitions each consecutive Fiver run into
  estimated-medium-sized groups, merges and converts those groups in parallel,
  and then performs at most one final Hazel merge.
- Each Fiver group removes its source Fivers only after its replacement Hazel
  has been published. The final Hazel merge similarly removes its source
  Hazels after publication, so consolidation cleans up as it progresses.
- A coherent `merge.*` recovery covering the final consolidation input is
  resumed. Unused, invalid, legacy, or conflicting recoveries are discarded.
  Cleanup uses the explicit `remove_hazel_merge_segments(...)` operation.
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

### Implemented SimplePosting and Hazel-Loop Cleanup

The cleanup remains small and preserves the current compact representation:

- Empty `qostings_` means every annotation is a point (`q == p`).
- Empty `fostings_` means every feature value is zero.
- Expanded vectors may still contain points or zero values alongside intervals
  or nonzero values.

Table-driven coverage now exercises the four source shapes:
point/no-value, point/value, interval/no-value, and interval/value. It exercises
all ordered pairs through both appendable and interleaved merges. It verifies
the result triples and invariants, serializes and decodes each result, and
inspects the serialized record to ensure qostings and fostings remain absent
whenever the merged data permits it. Existing focused coverage for duplicate
resolution and exclusion filtering remains in place.

Three local `SimplePosting` changes are implemented:

1. The ordered `posting_from_merge(...)` fast path uses the existing bulk
   `append(...)` operation instead of copying every entry through `get(...)`
   and `push(...)`. The existing append logic already expands missing
   qostings with `p` and missing fostings with zero only when another input
   requires the expanded representation.
2. `posting_from_compressed_blob(...)` and `posting_from_file(...)` resize only
   the vectors present in the serialized record and decompress directly into
   their storage. This removes the temporary decoded array and per-element
   `push_back(...)` copies.
3. `SimplePosting::write(...)` uses a small thread-local scratch object holding
   posting, qosting, and fvalue byte buffers. It grows qosting and fvalue
   buffers only when those vectors are present. This removes per-list heap
   allocation without changing the public API or file format.

Two small Hazel-driver changes are implemented:

- One source-posting vector is reused across features rather than allocated in
  every loop iteration. It reserves only enough room for one posting per input
  Hazel.
- The already maintained sequential directory positions identify and load
  sources for the current feature instead of binary-searching every Hazel
  directory again. Matching positions are consumed before advancing them, so
  checkpoint resume behavior remains unchanged.

The special null-feature and text-chunk paths remain unchanged; each runs at
most once and its existing lookup code is clearer. The checkpoint vector is
not broadly reserved from the sum of source-directory sizes because that upper
bound can substantially exceed the number of output features.

The library, aggregate tests, focused Hazel tests, build applications, and
standalone conversion/merge applications compile. Runtime regressions remain
with the user.

### SimplePosting and Hazel-Loop Benchmark Result

Against the immediately preceding zlib-reuse run, the user's `build.sh`
measurement reduced foreground consolidation from 1,259,340 ms to 1,231,490
ms, an improvement of 27,850 ms (2.21%). The phase comparison is:

- Fiver loading: 29,638 ms to 23,919 ms, down 5,719 ms (19.30%).
- Wide Fiver merge: 59,379 ms to 59,225 ms, effectively unchanged.
- Fiver-to-Hazel conversion: 501,577 ms to 501,426 ms, effectively unchanged.
- Hazel merge: 661,538 ms to 639,682 ms, down 21,856 ms (3.30%).

Peak resident memory fell from 16,837,072 KiB to 15,913,188 KiB, about 5.49%,
although a single-run memory comparison may include normal variation. Meadow
creation was unchanged, while foraging moved from 3:12.02 to 3:03.53.

The phase results fit the intended changes: direct decode materially helps
Fiver loading, and sequential directory consumption plus vector reuse helps
Hazel merging. Bulk append and reusable output buffers did not materially
change the wide Fiver merge or Fiver conversion. Further posting-allocation
micro-cleanup is therefore unlikely to address the remaining 501-second
conversion and 640-second Hazel merge times.

The posting format, singleton encoding, and exclusion semantics remained
unchanged through the following Hazel checkpoint and recovery replacement.

## Restartable Parallel Hazel Merge

The old `mrg.*`/`pst.*`/`dct.*` checkpoint path has been replaced by one
canonical two-pass Hazel merge used by both background pair merging and
foreground consolidation.

### Implemented Procedure

- Merge inputs must have compatible DNA/compressors and increasing,
  non-overlapping sequence ranges. Gaps are valid.
- Exactly two Hazels use one posting worker. Three or more use
  `allowed_threads(0)`, capped by feature work.
- Restart files are `merge.<segment>.<start>.<end>`. Each is an append-only,
  feature-ordered stream of complete `SimplePosting` records.
- `null_feature` is merged serially and becomes immutable exclusion input.
  The adjusted text-chunk posting is also prepared before worker execution and
  emitted when its sorted feature position is claimed.
- Ordinary features are dynamically claimed. Workers read source records by
  directory position, use the existing `SimplePosting` merge semantics, and
  write one record per completed feature.
- Final assembly performs a k-way feature scan over the segment logs. Empty
  completion records are omitted, eligible singletons use Hazel's inline
  representation, and other compressed posting records are copied without
  decompression or recompression.
- Text chunks retain the existing compressed-copy merge. The finished Hazel is
  assembled through an ordinary `.tmp` output and published before sources or
  restart segments are removed. No `mrg.*` marker is created.

### Recovery and Sanitization

- A segment tail may be truncated. Recovery scans complete headers and
  payloads, truncates only the partial tail, and resumes missing features.
- The segment count is fixed by the existing contiguous segment group.
  Recovery may run fewer workers, leaving excess segments frozen but valid.
- Legacy `mrg.*`, `pst.*`, and `dct.*` remnants are deleted.
- Startup first removes generic temporary files and kittens, sanitizes Fivers
  and Hazels, deletes Fivers fully shadowed by one Hazel, and rejects
  unexplained shard overlaps. Sequence gaps are retained.
- A valid partial merge must map to an existing consecutive set of Hazel
  sources. Invalid recoveries are removed. If partial merges conflict, every
  conflicting group is removed.
- Coherent recoveries are communicated through `HazelMergeRecovery`, including
  target range, source Hazels, and durable segment count. Background merging
  stores them in Fluffle and prefers them before new Hazel work.
- Foreground consolidation reuses a recovery only when it matches the complete
  final Hazel input; other recoveries are removed.

### Consolidation Pipeline

Foreground consolidation now sweeps each consecutive Fiver run greedily,
closing a group when its estimated size reaches `medium_shard` and retaining a
possibly small final group. Groups are merged and converted to Hazel in
parallel. Each group removes its source Fivers after publication. All resulting
and pre-existing Hazels are then merged by the same restartable Hazel procedure,
after which the source Hazels are removed.

The user's MS MARCO run processed 29 Fiver groups with 29 workers:

- opening and sanitizing: 299 ms;
- loading Fivers: 24,095 ms;
- parallel Fiver merge and conversion: 32,002 ms;
- merging 37 Hazels: 260,583 ms;
- final sanitization: 30 ms;
- total consolidation: 317,915 ms.

This reduced the foreground stage from roughly 14 minutes in the preceding
serial-conversion run to 5:18.68 while also reducing peak memory. The completed
directory contained one final Hazel and no stale source shards.

### Verification

- The user reports that `make testing` passes.
- Focused recovery coverage includes valid resume, incomplete segment groups,
  aborted transactions and sequence gaps, Fivers appearing in a recovery gap,
  conflicting partial merges, legacy cleanup, and idempotent segment removal.
- The user exercised background merging by repeatedly running `fluffy`,
  interrupting it, and ranking the evolving MS MARCO index; recovery and
  continued merging behaved correctly.
- The final MS MARCO Hazel ranked all 6,980 queries and emitted only the two
  established fake-result topics. Repeated runs of one index are stable.
  Differences between independently built indexes are dominated by equal-score
  ordering and top-10 boundary churn, not evidence of a merge failure.

The completed consolidation work and its follow-up cleanup are committed and
verified. There is no remaining consolidation step recorded here.
