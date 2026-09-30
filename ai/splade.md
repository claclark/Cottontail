# SPLADE performance measurements

Record user-run measurements here so later implementations can be compared
with their baselines. Timings are observations, not performance guarantees.
Keep effectiveness alongside speed and distinguish ranking timers from
whole-process wall time. No agent-run benchmarks are authorized by this file.

## SPLADE baseline

Recorded 2026-09-30, from the user's initial full MS MARCO passage run.

- Index: `splade.meadow`.
- Queries: `splade/splade_dev_query_vectors.jsonl`; 6,980 queries evaluated.
- Implementation: initial `src/splade.cc`, scanning all query hoppers to find
  each candidate container and again to score it; bounded top-k heap and
  parallelism across queries. No ordered-hopper list or pruning.
- Reported ranking time: 1,308,305 ms (21 min 48.305 sec).
- Derived throughput: 6,980 / 1,308.305 = approximately 5.34 queries/sec.
- Amortized time: approximately 187.44 ms/query across the parallel run;
  this is not individual query latency.
- MRR@10: **0.3832077477600405**, evaluated with
  `/data/ssd1/claclark/MARCO-2025/msmarco_passage_eval.py` and
  `/data/ssd1/claclark/MARCO-2025/qrels.dev.small.tsv`, after `hack.py`
  converted the TREC output to MS MARCO format.

The ranking timer reports the longest worker's ranking duration, not complete
process wall time. The exact invocation, effective thread count, search depth,
build flags, hardware, and cache state were not captured with this measurement.
The suggested command used depth 10; the CLI default was also changed to 10,
but the exact binary and invocation used for this run were not confirmed.
The library default remains 1000. Source changes were uncommitted.

For subsequent runs, record those settings, the implementation revision,
whole-process elapsed time when available, ranking time, query count, and
MRR@10. Keep this baseline rather than replacing it with a newer result.

## Linked list implementation

Implemented 2026-09-30 after user authorization. Hopper nodes live in a fixed vector for ownership and are
threaded with raw next pointers into a list ordered by current position.
The ranker finds the document containing the head, scores the prefix in that
document, then advances and reinserts those hoppers. It caches query weights
and current weighted contributions. Containers must not overlap and vector
labels must be unique within each document. There is no threshold window or
WAND pruning. Contributions now sum in position order rather than query-label
order, so floating-point rounding can differ from the baseline.

### Full collection run

User-reported on 2026-09-30, on host `clarke-gpu`:

```sh
time ./bazel-bin/apps/splade --verbose --meadow splade.meadow --depth 10 splade/splade_dev_query_vectors.jsonl > splade.rank
python3 hack.py < splade.rank > temp1.rank
python3 /data/ssd1/claclark/MARCO-2025/msmarco_passage_eval.py /data/ssd1/claclark/MARCO-2025/qrels.dev.small.tsv temp1.rank
```

- Reported ranking time: **529,972 ms**, versus baseline **1,308,305 ms**.
- Whole-process time: real **8m55.492s** (535,492 ms), user **239m26.332s**,
  sys **0m36.615s**.
- Queries evaluated: **6,980**; depth **10**; automatic thread selection
  (effective worker count not recorded).
- Ranking-timer throughput: **13.17 queries/sec**; amortized **75.93 ms/query**,
  not individual query latency.
- Observed ranking-timer speedup: **2.47x**, or **59.49% less time**.
  Baseline invocation and cache state were not captured, so this is an
  observed comparison, not a controlled benchmark.
- MRR@10: **0.3832077477600405**, identical to the baseline's reported value.
  Identical aggregate MRR does not establish identical per-query rankings.

The optimized app and focused tests compiled before this run; the broader
regression suite for the linked-list change has not yet been reported.
