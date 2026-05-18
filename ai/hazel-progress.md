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
