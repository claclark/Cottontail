# Hazel Progress Log

This file tracks Hazel activation and performance milestones so changes can be
compared against concrete outcomes.

## 2026-05-18: Correctness Baseline Under `rank.sh`

Test:

```
./rank.sh a.meadow/hazel.00000000000000000000.00000000000000000058
```

Pipeline:

```
bm25:b=0.68 bm25:k1=0.82 bm25:depth=10 stop stem bm25
```

Workload:

- MARCO dev small queries.
- `--threads 54`.
- Standalone Hazel shard rebuilt from `a.meadow` Fiver.

Result:

- Ranking completed successfully.
- `MRR @10: 0.18923028380406587`.
- `QueriesRanked: 6980`.
- Same missing-result topic as Fiver-backed run: `645252`.

Timing/resource output:

- Internal ranking timer: `2584363` ms.
- Wall time: `43:36.18`.
- User time: `13336.86`.
- System time: `20907.34`.
- CPU: `1308%`.
- Max RSS: `30653668` KB.
- Minor page faults: `3335578307`.

Comparison point from Fiver-backed `a.meadow` run in the same session:

- Internal ranking timer: `6850` ms.
- Wall time: `1:43.20`.
- `MRR @10: 0.18923028380406587`.
- `QueriesRanked: 6980`.
- Note: most of the Fiver-backed wall time is loading the Fiver into memory.
  Once loaded, queries run quickly. The internal ranking timer is the better
  comparison for hot query execution.

Interpretation:

- First-pass Hazel activation is functionally correct for this BM25 workload.
- Performance is not acceptable yet.
- Very high system time and page faults are consistent with repeated posting
  reads/decompression, broad locking, and inefficient cache behavior.
- The key gap is not just total process wall time. Hazel's internal ranking
  timer is also orders of magnitude slower than the Fiver-backed hot ranking
  timer.

Current suspected bottlenecks:

- `HazelIdx::hopper(feature)` repeatedly reads and decompresses idx entries
  with no decoded posting cache.
- `HazelTxt::translate(...)` serializes too much work behind one mutex.
- Shallow Hazel cloning causes worker threads to share the same `HazelTxt`
  object, text-chunk hopper, and translate mutex.
- `Stats` clones likely repeat metadata/stat hopper construction, amplifying
  the no-cache idx behavior.

Next starting point:

- Review and wire in `src/read_gate.h`.
- `ReadGate` is a small header-only helper that owns one file descriptor, uses
  `pread`, and allows two concurrent reads by default.
- It returns a `std::unique_ptr<char[]>` because callers immediately
  decompress/decode and discard the compressed buffer.
- Its nested `Permit` class is an RAII guard: constructing it acquires one read
  slot, and destruction releases the slot on all return paths.
- `ReadGate` intentionally does no caching, coalescing, decompression, or worker
  thread management. In-flight deduplication belongs in the future idx/txt
  caches above the read layer.

Next measurements should be added here after each meaningful optimization.

## 2026-05-20: HazelTxt Rebuild Ranker Check

Test:

```
./rank.sh a.meadow/hazel.00000000000000000000.00000000000000000058
```

Pipeline:

```
bm25:b=0.68 bm25:k1=0.82 bm25:depth=10 stop stem bm25
```

Workload:

- MARCO dev small queries.
- `--threads 54`.
- Same standalone Hazel shard as the 2026-05-18 baseline.
- Rank binary was `bazel-bin/apps/working`.

Result:

- Ranking completed successfully after the `HazelTxt` rebuild.
- `MRR @10: 0.18923028380406587`.
- `QueriesRanked: 6980`.
- Same missing-result topic as earlier runs: `645252`.

Timing/resource output:

- Internal ranking timer: `2566150` ms.
- Wall time: `43:17.83`.
- User time: `13174.75`.
- System time: `20697.36`.
- CPU: `1303%`.
- Max RSS: `32068276` KB.
- Minor page faults: `3249193947`.

Interpretation:

- Correctness remains consistent with the earlier Hazel and Fiver-backed runs.
- The HazelTxt rebuild did not materially change the overall ranker runtime for
  this workload.
- This supports the current suspicion that the next major bottleneck is still
  repeated HazelIdx posting reads/decompression rather than text translation.

## 2026-05-21: HazelIdx Decoded Posting Cache Check

Test:

```
./rank.sh a.meadow/hazel.00000000000000000000.00000000000000000058
```

Pipeline:

```
bm25:b=0.68 bm25:k1=0.82 bm25:depth=10 stop stem bm25
```

Workload:

- MARCO dev small queries.
- `--threads 54`.
- Same standalone Hazel shard as the earlier baseline runs.
- Rank binary was `bazel-bin/apps/working`.

Result:

- Ranking completed successfully after the first HazelIdx decoded posting cache.
- `MRR @10: 0.18923028380406587`.
- `QueriesRanked: 6980`.
- Same missing-result topic as earlier runs: `645252`.

Timing/resource output:

- Internal ranking timer: `12794` ms.
- Wall time: `0:43.78`.
- User time: `236.63`.
- System time: `7.89`.
- CPU: `558%`.
- Max RSS: `5655672` KB.
- Minor page faults: `1504162`.

Interpretation:

- Correctness remains consistent with the earlier Hazel and Fiver-backed runs.
- The first HazelIdx decoded posting cache removes the dominant repeated
  posting read/decompression bottleneck for this workload.
- Internal ranking time improved from `2566150` ms after the HazelTxt rebuild
  to `12794` ms with the HazelIdx cache, a roughly 200x improvement.
- Wall time improved from `43:17.83` to `0:43.78`.

## 2026-05-21: HazelIdx ReadGate 16 Check

Test:

```
./rank.sh a.meadow/hazel.00000000000000000000.00000000000000000058
```

Pipeline:

```
bm25:b=0.68 bm25:k1=0.82 bm25:depth=10 stop stem bm25
```

Workload:

- MARCO dev small queries.
- `--threads 54`.
- Same standalone Hazel shard as the earlier baseline runs.
- Rank binary was `bazel-bin/apps/working`.
- HazelIdx `ReadGate` reader count: `16`.
- HazelTxt `ReadGate` reader count: default `2`.

Result:

- Ranking completed successfully with the HazelIdx read gate at 16 readers.
- `MRR @10: 0.18923028380406587`.
- `QueriesRanked: 6980`.
- Same missing-result topic as earlier runs: `645252`.

Timing/resource output:

- Internal ranking timer: `11850` ms.
- Wall time: `0:42.91`.
- User time: `236.55`.
- System time: `7.50`.
- CPU: `568%`.
- Max RSS: `5648272` KB.
- Minor page faults: `1502386`.

Interpretation:

- Correctness remains consistent with the earlier Hazel and Fiver-backed runs.
- Increasing the HazelIdx read gate to 16 gives a modest improvement over the
  first cache result: `12794` ms to `11850` ms internally, and `0:43.78` to
  `0:42.91` wall time.
- The major gain still comes from decoded posting caching; read-gate tuning is
  useful but secondary on this workload.

## 2026-05-21: Hazel ReadGate 16/16 Check

Test:

```
./rank.sh a.meadow/hazel.00000000000000000000.00000000000000000058
```

Pipeline:

```
bm25:b=0.68 bm25:k1=0.82 bm25:depth=10 stop stem bm25
```

Workload:

- MARCO dev small queries.
- `--threads 54`.
- Same standalone Hazel shard as the earlier baseline runs.
- Rank binary was `bazel-bin/apps/working`.
- HazelIdx `ReadGate` reader count: `16`.
- HazelTxt `ReadGate` reader count: `16`.
- Other workload was running on the box, so treat this as a noisy comparison.

Result:

- Ranking completed successfully with both Hazel read gates at 16 readers.
- `MRR @10: 0.18923028380406587`.
- `QueriesRanked: 6980`.
- Same missing-result topic as earlier runs: `645252`.

Timing/resource output:

- Internal ranking timer: `12111` ms.
- Wall time: `0:43.30`.
- User time: `236.23`.
- System time: `7.97`.
- CPU: `563%`.
- Max RSS: `5649708` KB.
- Minor page faults: `1502662`.

Interpretation:

- Correctness remains consistent with the earlier Hazel and Fiver-backed runs.
- The 16/16 read-gate result is slightly slower than the HazelIdx-only 16-reader
  run (`12111` ms vs. `11850` ms internally), but the host was not quiet.
- This is close enough for now; the next useful work is Bigwig/Hazel integration
  rather than further gate tuning.

## 2026-05-21: Hazel ReadGate 16/16 With 128 Ranker Threads

Test:

```
./rank.sh a.meadow/hazel.00000000000000000000.00000000000000000058
```

Pipeline:

```
bm25:b=0.68 bm25:k1=0.82 bm25:depth=10 stop stem bm25
```

Workload:

- MARCO dev small queries.
- `--threads 128`.
- Same standalone Hazel shard as the earlier baseline runs.
- Rank binary was `bazel-bin/apps/working`.
- HazelIdx `ReadGate` reader count: `16`.
- HazelTxt `ReadGate` reader count: `16`.

Result:

- Ranking completed successfully with 128 ranker threads.
- `MRR @10: 0.18923028380406587`.
- `QueriesRanked: 6980`.
- Same missing-result topic as earlier runs: `645252`.

Timing/resource output:

- Internal ranking timer: `11801` ms.
- Wall time: `0:42.88`.
- User time: `236.22`.
- System time: `7.35`.
- CPU: `568%`.
- Max RSS: `5651412` KB.
- Minor page faults: `1503001`.

Interpretation:

- Correctness remains consistent with the earlier Hazel and Fiver-backed runs.
- Increasing ranker threads from 54 to 128 did very little on this workload:
  internal time moved from roughly `12111` ms to `11801` ms and wall time from
  `0:43.30` to `0:42.88`.
- CPU stayed around `568%`, so simple ranker-thread count is not the current
  utilization lever.

## 2026-05-21: Hazel ReadGate 16/16 With 1 Ranker Thread

Test:

```
./rank.sh a.meadow/hazel.00000000000000000000.00000000000000000058
```

Pipeline:

```
bm25:b=0.68 bm25:k1=0.82 bm25:depth=10 stop stem bm25
```

Workload:

- MARCO dev small queries.
- `--threads 1`.
- Same standalone Hazel shard as the earlier baseline runs.
- Rank binary was `bazel-bin/apps/working`.
- HazelIdx `ReadGate` reader count: `16`.
- HazelTxt `ReadGate` reader count: `16`.

Result:

- Ranking completed successfully with one ranker thread.
- `MRR @10: 0.18923028380406587`.
- `QueriesRanked: 6980`.
- Same missing-result topic as earlier runs: `645252`.

Timing/resource output:

- Internal ranking timer: `172635` ms.
- Wall time: `3:23.65`.
- User time: `201.42`.
- System time: `5.28`.
- CPU: `101%`.
- Max RSS: `5624428` KB.
- Minor page faults: `1493724`.

Interpretation:

- Correctness remains consistent with the earlier Hazel and Fiver-backed runs.
- The single-thread run provides a useful scaling anchor: internal time is
  `172635` ms versus roughly `11800`-`12100` ms at high thread counts.
- Ranking work does parallelize substantially, but it plateaus far below 128-way
  utilization on this workload.
