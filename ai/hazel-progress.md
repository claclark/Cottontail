# Hazel Progress Log

This file tracks Hazel activation and performance milestones so changes can be
compared against concrete outcomes.

Entries are historical snapshots. Some sections intentionally describe states,
commands, binaries, and next steps that were later superseded. For current
behavior and the active design checkpoint, see `ai/hazel.md`,
`ai/hazel-merge-notes.md`, `ai/notes.md`, and `ai/plan.md`.

## 2026-06-16: Three-Fiver Bigwig After OwslaCache

Context:

- Bigwig merged-posting cache generations now use `OwslaCache`, which installs
  a waitable `SimplePosting` before filling. The winning caller fills the
  posting; other callers wait on the same storage instead of independently
  rebuilding the same merged posting.
- The old feature-level `Fiver::merge(...)` hopper/cache helper has been
  removed; BigwigIdx owns visible-read feature posting composition.
- Test burrow was the three-Fiver `a.meadow`.

Run:

```
./rank.sh a.meadow
```

- Max worker ranking-loop time: `9062` ms.
- Wall time: `1:18.15`.
- User time: `255.32`.
- System time: `15.65`.
- CPU: `346%`.
- Max RSS: `21677512` KB.
- Major page faults: `0`.
- Minor page faults: `7258753`.
- `MRR @10: 0.18975923272843034`.
- `QueriesRanked: 6980`.
- Fake result topics: `645252`, `970152`.

Comparison with the 2026-06-07 three-Fiver `a.meadow` record:

- Max worker ranking-loop time improved from `25891` ms to `9062` ms
  (`2.86x` faster).
- Wall time improved from `1:46.29` to `1:18.15`.
- User time dropped from `584.06` to `255.32`.
- System time dropped from `96.51` to `15.65`.
- Max RSS dropped from `67673128` KB to `21677512` KB.
- Minor page faults dropped from `25829605` to `7258753`.
- MRR and query count were unchanged.

Interpretation:

- The large improvement is consistent with removing repeated concurrent
  multi-Fiver merged-posting construction during ranking.
- The three-Fiver path is now close to the old single-Fiver `b.meadow` memory
  footprint (`21677512` KB vs. `21804204` KB recorded on 2026-06-07), though
  its hot ranking loop remains slower (`9062` ms vs. `6980` ms).
- The merged Hazel path still has much lower end-to-end wall time and memory on
  the earlier record, but its hot ranking loop was slower than this updated
  three-Fiver path (`12583` ms vs. `9062` ms).

Follow-up after async cache fill and Bigwig posting-factory ownership cleanup:

```
./rank.sh a.meadow
```

- Max worker ranking-loop time: `9209` ms.
- Wall time: `1:19.63`.
- User time: `259.66`.
- System time: `17.01`.
- CPU: `347%`.
- Max RSS: `21808088` KB.
- Major page faults: `0`.
- Minor page faults: `7297867`.
- `MRR @10: 0.18975923272843034`.
- `QueriesRanked: 6980`.
- Fake result topics: `645252`, `970152`.

Comparison with the earlier 2026-06-16 post-`OwslaCache` run:

- Max worker ranking-loop time changed from `9062` ms to `9209` ms.
- Wall time changed from `1:18.15` to `1:19.63`.
- Max RSS changed from `21677512` KB to `21808088` KB.
- MRR and query count were unchanged.
- Interpretation: the async fill / factory ownership cleanup left the
  three-Fiver ranking profile essentially unchanged, while preserving the large
  gain over the 2026-06-07 three-Fiver baseline.

Single-Fiver regression check after the same cache/Owsla work:

```
./rank.sh b.meadow
```

- Max worker ranking-loop time: `6706` ms.
- Wall time: `1:15.09`.
- User time: `231.79`.
- System time: `11.94`.
- CPU: `324%`.
- Max RSS: `21598456` KB.
- Major page faults: `0`.
- Minor page faults: `6278874`.
- `MRR @10: 0.1896242666120888`.
- `QueriesRanked: 6980`.
- Fake result topics: `645252`, `970152`.

Comparison with the 2026-06-07 single-Fiver `b.meadow` record:

- Max worker ranking-loop time changed from `6980` ms to `6706` ms.
- Wall time changed from `1:22.42` to `1:15.09`.
- Max RSS changed from `21804204` KB to `21598456` KB.
- MRR and query count were unchanged.
- Interpretation: the single-Fiver path remains stable; the large cache-related
  improvement is specific to multi-shard Bigwig reads.

Follow-up regression checks after making Hazel inherit `Owsla` and replacing
the multi-Fiver `text_chunk_tag` `VectorHopper` path with a fresh uncached
posting merge:

```
./rank.sh a.meadow/
```

- Max worker ranking-loop time: `9256` ms.
- Wall time: `1:18.60`.
- User time: `257.00`.
- System time: `17.38`.
- CPU: `349%`.
- Max RSS: `21824160` KB.
- Major page faults: `0`.
- Minor page faults: `7295545`.
- `MRR @10: 0.18975923272843034`.
- `QueriesRanked: 6980`.
- Fake result topics: `645252`, `970152`.

```
./rank.sh b.meadow/
```

- Max worker ranking-loop time: `6586` ms.
- Wall time: `1:13.91`.
- User time: `230.79`.
- System time: `12.19`.
- CPU: `328%`.
- Max RSS: `21598360` KB.
- Major page faults: `0`.
- Minor page faults: `6278870`.
- `MRR @10: 0.1896242666120888`.
- `QueriesRanked: 6980`.
- Fake result topics: `645252`, `970152`.

```
./rank.sh a.meadow/hazel.00000000000000000000.00000000000000000058
```

- Max worker ranking-loop time: `12455` ms.
- Wall time: `0:13.95`.
- User time: `205.50`.
- System time: `9.06`.
- CPU: `1537%`.
- Max RSS: `5959800` KB.
- Major page faults: `0`.
- Minor page faults: `1909489`.
- `MRR @10: 0.18975923272843034`.
- `QueriesRanked: 6980`.
- Fake result topics: `645252`, `970152`.

Interpretation:

- The three-Fiver `a.meadow` path remains in the same range as the earlier
  post-async-cache-fill run (`9256` ms vs. `9209` ms) with unchanged
  MRR/query count.
- The single-Fiver `b.meadow` path remains stable and slightly faster than the
  earlier 2026-06-16 `6706` ms observation.
- The merged Hazel path remains consistent with earlier merged-Hazel ranking
  checks and preserves the same MRR/query-count profile.

Follow-up regression checks after the Bigwig startup `sanitize(...)` refactor:

- `a.meadow/` with three Fivers and no Hazel files: max worker ranking-loop
  time `9382` ms, wall `1:18.99`, max RSS `21817452` KB,
  `MRR @10: 0.18975923272843034`, `QueriesRanked: 6980`.
- `b.meadow/` with one Fiver covering `0..58`: max worker ranking-loop time
  `6671` ms, wall `1:14.29`, max RSS `21598504` KB,
  `MRR @10: 0.1896242666120888`, `QueriesRanked: 6980`.
- `c.meadow/save3` as a standalone merged Hazel comparison: max worker
  ranking-loop time `12571` ms, wall `0:14.07`, max RSS `5988696` KB,
  `MRR @10: 0.18975923272843034`, `QueriesRanked: 6980`.
- After running ranking until Fiver consolidation occurred and restarting
  several times, `a.meadow/` contained only `fiver.0.37` and `fiver.38.58`
  (with zero-padded names), no Hazel or temporary sidecars, and ranked with
  max worker ranking-loop time `9079` ms, wall `1:13.09`, max RSS
  `23907236` KB, `MRR @10: 0.18975923272843034`, `QueriesRanked: 6980`.
- Interpretation: for clean Fiver-only directories, the sanitizer refactor
  preserves the previous activation and Fiver-to-Fiver merge behavior. Hazel
  activation remains the next wiring step.

## 2026-06-13: Repeated Restart Hazel Merge Smoke

Context:

- The focus was the `HazelIdx::merge` restart path: repair existing
  `dst.pst`/`dst.dct` checkpoint sidecars, reestablish the processed-feature
  invariant, and continue.
- `a.meadow` still contained the same three input Hazel shards covering
  sequence ranges `0..11`, `12..37`, and `38..58`.
- The final expected merged Hazel size was known from saved full outputs.

Manual interruption sequence:

```
./bazel-bin/apps/fiver2hazel --merge a.meadow
^C
./bazel-bin/apps/fiver2hazel --merge a.meadow
^C
./bazel-bin/apps/fiver2hazel --merge a.meadow
^C
./bazel-bin/apps/fiver2hazel --merge a.meadow
```

Completed run:

- Merge time: `500234` ms.
- Total time: `500238` ms.
- Final Hazel:
  `a.meadow/hazel.00000000000000000000.00000000000000000058`.
- Final Hazel size: `3219314852` bytes.
- Saved comparison files `save0`, `save1`, `save2`, and `save3` were also
  `3219314852` bytes.

Follow-up rank check:

```
./rank.sh a.meadow/hazel.00000000000000000000.00000000000000000058
```

- Ranker-reported ranking time: `12114` ms.
- Wall time: `0:13.54`.
- User time: `206.08`.
- System time: `8.12`.
- CPU: `1581%`.
- Max RSS: `5657480` KB.
- Major page faults: `2`.
- Minor page faults: `1504918`.
- `MRR @10: 0.18975923272843034`.
- `QueriesRanked: 6980`.
- Fake result topics: `645252`, `970152`.

Live checkpoint snapshot observed before the final successful publish:

- `dst.dct`: `247356528` bytes.
- `dst.pst`: `1008524086` bytes.
- `dst.tmp`: `0` bytes.

Interpretation:

- Repeated process interruption and restart completed and published the final
  Hazel.
- The final output size matched prior saved merged outputs.
- The ranking profile matched the earlier clean and resumed Hazel results.
- A large `.dct` relative to `.pst` is plausible for this format: each
  directory entry is three `addr` values, and inline singleton postings are
  represented by directory-only entries.
- This is evidence for robust process-kill/restart behavior under the
  append-prefix sidecar assumption. It is not intended as a separate durability
  guarantee beyond normal local-filesystem crash consistency.

## 2026-06-11: Restartable Hazel Merge Smoke Timing

Context:

- Restartable activated-Hazel merge has replaced the old file-parsing merge
  behind the `apps/fiver2hazel --merge` path.
- `a.meadow` contains three input Hazel shards covering sequence ranges
  `0..11`, `12..37`, and `38..58`.
- `fiver2hazel` discovery now ignores non-shard sidecar names such as
  checkpoint `.dct`/`.pst` files.

Clean no-interruption merge:

```
./bazel-bin/apps/fiver2hazel --merge a.meadow/
```

- Merge time: `707052` ms.
- Total time: `707056` ms.
- Final Hazel:
  `a.meadow/hazel.00000000000000000000.00000000000000000058`.

Follow-up rank check:

```
./rank.sh a.meadow/hazel.00000000000000000000.00000000000000000058
```

- Max worker ranking-loop time: `12533` ms.
- Wall time: `0:14.04`.
- User time: `206.49`.
- System time: `9.50`.
- CPU: `1538%`.
- Max RSS: `5655848` KB.
- Major page faults: `2`.
- Minor page faults: `1504365`.
- `MRR @10: 0.18975923272843034`.
- `QueriesRanked: 6980`.
- Fake result topics: `645252`, `970152`.

Interrupted/resumed smoke check:

- A merge was killed partway through, then restarted with the same command.
- Resumed merge time: `1044534` ms.
- Total resumed time: `1044538` ms.
- The final Hazel size was `3219314852` bytes, matching saved old-merge-sized
  outputs.
- Follow-up rank check matched the same MRR/query-count profile:
  `MRR @10: 0.18975923272843034`, `QueriesRanked: 6980`, fake result topics
  `645252` and `970152`.

Interpretation:

- Correctness looks stable across both clean and interrupted/resumed merge
  paths on this workload.
- The clean restartable merge is in the same neighborhood as the earlier
  `718082` ms clean observation, but remains slower than the old kept
  unique-posting raw-copy implementation (`594594` ms).
- The interrupted/resumed timing confirms that checkpointing avoided restarting
  from scratch, but the successful resumed path still pays final idx assembly
  and full txt copy costs.

## 2026-06-07: Fiver/Hazel Blend Motivation Under New TREC Timer

After adding `trec(..., addr *time = nullptr)`, `apps/rank --verbose` reports
the maximum per-worker ranking-loop time. This is different from older
`Ranking took:` entries in this file, which measured the outer `trec(...)`
call. External `/usr/bin/time` wall/user/system/RSS values remain the best
cross-era comparison.

Common workload:

```
./rank.sh <burrow>
```

Pipeline:

```
bm25:b=0.68 bm25:k1=0.82 bm25:depth=10 stop stem bm25
```

Workload:

- MARCO dev small queries.
- Requested `--threads 2000` (`trec` still caps internally by hardware and
  query count).
- Rank binary was `bazel-bin/apps/rank`.

Merged Hazel from `a.meadow`:

```
./rank.sh a.meadow/hazel.00000000000000000000.00000000000000000058
```

- Max worker ranking-loop time: `12583` ms.
- Wall time: `0:14.08`.
- User time: `204.41`.
- System time: `10.15`.
- CPU: `1523%`.
- Max RSS: `5659140` KB.
- Major page faults: `1`.
- Minor page faults: `1505372`.
- `MRR @10: 0.18975923272843034`.
- `QueriesRanked: 6980`.
- Fake result topics: `645252`, `970152`.

Three-Fiver `a.meadow`:

```
./rank.sh a.meadow
```

- Max worker ranking-loop time: `25891` ms.
- Wall time: `1:46.29`.
- User time: `584.06`.
- System time: `96.51`.
- CPU: `640%`.
- Max RSS: `67673128` KB.
- Major page faults: `0`.
- Minor page faults: `25829605`.
- `MRR @10: 0.18975923272843034`.
- `QueriesRanked: 6980`.
- Fake result topics: `645252`, `970152`.

Single-Fiver `b.meadow`:

```
./rank.sh b.meadow
```

- Max worker ranking-loop time: `6980` ms.
- Wall time: `1:22.42`.
- User time: `234.15`.
- System time: `14.37`.
- CPU: `301%`.
- Max RSS: `21804204` KB.
- Major page faults: `0`.
- Minor page faults: `6313859`.
- `MRR @10: 0.1896242666120888`.
- `QueriesRanked: 6980`.
- Fake result topics: `645252`, `970152`.

Interpretation:

- The large Fiver path can be very fast once hot (`b.meadow` loop around
  `7` seconds), but opening/loading large Fivers dominates wall time and memory
  (`82` seconds wall and about `21.8` GB RSS for `b.meadow`; `106` seconds wall
  and about `67.7` GB RSS for three-Fiver `a.meadow`).
- The merged Hazel path has a slower ranking loop than hot `b.meadow` on this
  workload, but its end-to-end wall time and memory footprint are much better
  (`14` seconds wall and about `5.7` GB RSS).
- These measurements motivate a carefully blended Fiver/Hazel Bigwig path:
  keep Fiver-like hot query behavior where it matters, while using Hazel to
  avoid the startup and memory cost of very large static Fivers.

## 2026-05-26: Meadowlark Stemmer/Tokenizer Fix Observation

After fixing Meadowlark tf-idf stats so ranking-view metadata/defaults initialize
the base `Stats` stemmer/tokenizer, the user reported:

- `MRR @10: 0.18975923272843034`.
- `QueriesRanked: 6980`.

This differs slightly from the earlier recorded `0.18923028380406587` baseline
with the same query count. Likely explanation discussed: tf-idf annotations and
query processing now agree on the tf-idf default tokenizer (`ascii`) and
metadata/default stemmer (`porter`).

## 2026-05-27: Fiver-To-Hazel And Hazel Merge Timing

Test:

```
./bazel-bin/apps/fiver2hazel a.meadow
```

Input burrow:

- `a.meadow` with three living Fivers covering sequence ranges `0..11`,
  `12..37`, and `38..58`.
- Default `fiver2hazel` behavior after adding `--convert` / `--merge`: remove
  existing Hazels, convert all living Fivers to per-Fiver Hazels, then merge the
  available Hazels into `hazel.00000000000000000000.00000000000000000058`.
- Conversion timing wraps only `fiver->hazel(...)`, after Fiver activation.
- Merge timing wraps `Hazel::merge(...)`, including input Hazel metadata and
  directory loading.

Reported timings:

- Converted `hazel.00000000000000000000.00000000000000000011` in `93179` ms.
- Converted `hazel.00000000000000000012.00000000000000000037` in `284439` ms.
- Converted `hazel.00000000000000000038.00000000000000000058` in `373654` ms.
- Total conversion time: `751272` ms.
- Hazel merge time: `611399` ms.
- Total end-to-end time: `1426308` ms.

Interpretation:

- These conversion timings are a stress case because future Bigwig integration
  should normally convert bounded-size active Fivers rather than multi-hundred
  MiB or GiB Fiver pickles.
- Hazel-to-Hazel merge is the more important long-term maintenance workload.
- Current Hazel merge is usable but still has clear optimization room: normal
  features present in only one input Hazel can be copied as raw compressed
  posting bytes, with inline singleton entries copied as directory-only
  entries, avoiding decompression and posting merge semantics.

Follow-up rank check:

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
- Fresh merged Hazel produced by the `fiver2hazel` run above.
- Rank binary was `bazel-bin/apps/working`.

Result:

- Ranking completed successfully.
- `MRR @10: 0.18975923272843034`.
- `QueriesRanked: 6980`.
- Fake result topics reported by the ranker: `645252` and `970152`.

Timing/resource output:

- Internal ranking timer: `11804` ms.
- Wall time: `0:43.04`.
- User time: `237.47`.
- System time: `8.49`.
- CPU: `571%`.
- Max RSS: `5655856` KB.
- Minor page faults: `1504232`.

Interpretation:

- The freshly rebuilt merged Hazel retains the post-cache fast query profile.
- The MRR matches the current Meadowlark stemmer/tokenizer-fix observation, so
  this run does not indicate a Hazel merge regression.

## 2026-05-28: Hazel Merge Unique-Posting Raw-Copy Check

Change:

- Added a Hazel merge idx fast path for normal features that appear in exactly
  one input Hazel when no `null_feature` exclusion posting is present.
- The fast path preserves inline singleton entries as directory-only output
  entries and raw-copies non-empty compressed posting byte ranges without
  decode/re-encode.
- `null_feature`, `text_chunk_tag`, excluded/deleted ordinary features, and
  multi-input ordinary features remain on the decoded semantic merge path.

Test:

```
./bazel-bin/apps/fiver2hazel a.meadow
```

Reported timings:

- Converted `hazel.00000000000000000000.00000000000000000011` in `93057` ms.
- Converted `hazel.00000000000000000012.00000000000000000037` in `284861` ms.
- Converted `hazel.00000000000000000038.00000000000000000058` in `373878` ms.
- Total conversion time: `751796` ms.
- Hazel merge time: `594594` ms.
- Total end-to-end time: `1409963` ms.

Comparison to the 2026-05-27 baseline:

- Previous Hazel merge time: `611399` ms.
- New Hazel merge time: `594594` ms.
- Merge improvement: `16805` ms, about `2.75%`.
- Previous end-to-end time: `1426308` ms.
- New end-to-end time: `1409963` ms.
- End-to-end improvement: `16345` ms, about `1.15%`.

Interpretation:

- The fast path is correct-looking and measurable, but modest.
- It likely reduces decode/re-encode work for unique features, while leaving
  the main I/O pattern largely unchanged: copied postings are still read one
  byte range at a time through `HazelMergeInput::read_at(...)`.
- The next likely optimization direction is sequential/buffered or asynchronous
  read-ahead for Hazel merge input ranges, especially for the common case of
  merging two Hazels.

## 2026-05-28: Hazel Merge Async I/O Experiment

Experiment:

- Added temporary double-buffered async I/O helpers and wired `AsyncReader` into
  Hazel merge for sequential txt chunk copying and idx posting reads.
- This preserved the existing Hazel merge semantics:
  - txt chunks were already copied as compressed bytes without decompression;
  - unique ordinary postings still used the raw-copy fast path;
  - `null_feature`, `text_chunk_tag`, and multi-input postings remained on the
    decoded semantic path.
- The test was intentionally run from HDD storage, where I/O effects should be
  easier to see than on SSD.

Reported timing:

```
./bazel-bin/apps/fiver2hazel a.meadow
```

- Converted `hazel.00000000000000000000.00000000000000000011` in `93101` ms.
- Converted `hazel.00000000000000000012.00000000000000000037` in `284477` ms.
- Converted `hazel.00000000000000000038.00000000000000000058` in `373987` ms.
- Total conversion time: `751565` ms.
- Removed `1` previously merged Hazel shard.
- Hazel merge time with async read-ahead: `588807` ms.
- Total end-to-end time: `1403275` ms.

Comparison:

- Previous post-raw-copy Hazel merge time: `594594` ms.
- Async read-ahead Hazel merge time: `588807` ms.
- Merge improvement: `5787` ms, about `1.0%`.

Conclusion:

- This experiment did not justify the added complexity in `hazel.cc`.
- Even on HDD, read-ahead produced only a small improvement; on SSD it is likely
  to be noise.
- Hazel merge is not primarily I/O-bound after compressed txt chunk streaming
  and the unique-posting raw-copy path. Remaining cost is more likely in
  multi-input posting decode/merge/recompression and special posting handling.
- The useful part of the surrounding work was the `apps/fiver2hazel` behavior
  change that preserves intermediate per-Fiver Hazels so conversion does not
  need to be repeated just to remeasure merge.
- Recommendation: keep the clean unique-posting raw-copy fast path and the
  `fiver2hazel` intermediate-Hazel preservation, but do not pursue async
  read-ahead in Hazel merge unless future measurements show a much larger I/O
  bottleneck.

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
