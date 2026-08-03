# Design Record: Restartable Parallel Hazel Merge

Status: implemented and verified at the 2026-07-26 commit checkpoint. Current
behavior is documented in `ai/consolidation.md`, `ai/notes.md`, and
`ai/hazel.md`; this file preserves the design review and decisions that led to
it.

## Overall Assessment

The two-pass design in `ai/consolidation.md` is a good fit for the measured
problem. It separates the expensive decode, semantic merge, exclusion
filtering, and compression work from the relatively cheap construction of the
final Hazel.
It also preserves one canonical merge procedure and the existing
`SimplePosting` representation rather than adding a foreground-only format.

The implementation incorporates the clarifications below. The most important
constraint remains that recovery is routine for large merges, not merely an
exceptional cleanup path.

## Confirmed Decisions

### Inputs and Threads

- Exactly two input Hazels use one posting worker.
- Three or more input Hazels use `allowed_threads(0)`, capped by the available
  feature work.
- The input count is the thread-policy switch. No separate foreground merge
  implementation is needed.
- Merge inputs carry increasing, non-overlapping Hazel sequence metadata.
  Gaps caused by aborted commits are valid. Supporting sequence-less activated
  merges is no longer required.
- Input Hazels must have compatible DNA and compressor recipes. Within a
  Bigwig, fixed configuration plus the recorded ordered source Hazel set is
  sufficient identity for recovered posting records; no additional manifest
  or fingerprint is required.

### Recovery and Lifecycle

- Valid `merge.*` posting-record groups are resumed. A large Hazel merge
  spanning multiple Bigwig activations is normal expected behavior.
- Legacy `pst.*`, `dct.*`, and `mrg.*` partial merges are unconditionally
  discarded.
- If recovery finds conflicting partial merges, discard every conflicting
  group and restart from the sanitized living Hazel inventory.
- If the completed target Hazel exists, it wins and all covered merge files
  and source Hazels may be cleaned up through the existing publication and
  sanitization lifecycle.
- The segment count is durable once posting work begins. If fewer workers are
  available after restart, excess segments stop growing but remain inputs to
  completion tracking and final assembly.
- No additional metadata, exclusion, or text files should be introduced.

### Filesystem Failure Model

Recovery assumes a healthy ordinary filesystem and process-level
interruption. Each segment has one sequential writer, and the visible file
after interruption is assumed to be a prefix of the intended byte stream:
every byte present was written by the merge, but the final record may be
truncated.

This is not a power-loss, media-corruption, or hostile-filesystem guarantee.
There is no per-record `fsync`, checksum, or attempt to detect a structurally
complete but corrupted payload.

- A partial final header or payload is truncated to the last complete record.
- A malformed record before the tail, a feature-order violation, or a
  duplicate feature across segments invalidates the recovery group.
- Segment streams are flushed and closed before final assembly.
- Records do not need to be flushed individually. Losing buffered complete
  records merely causes those features to be recomputed after restart.

## Recommended Merge Mechanics

### Serial Prerequisites

Create the contiguous posting-segment set before starting semantic posting
work. Use those same segment files for the special features:

1. If any source contains `null_feature`, merge it synchronously, write its
   ordinary `SimplePosting` record to segment zero, and retain the decoded
   merged posting as immutable shared exclusion input.
2. On recovery, reuse and decode the completed null-feature record. If it was
   not completed, restore an ordered append point and recompute it before
   ordinary workers start.
3. Construct the adjusted `text_chunk_tag` posting synchronously from the
   source postings and cumulative raw-text bases.
4. Hold that posting in memory until the ordered union enumerator reaches its
   feature, then write it as an ordinary segment record. It must not simply be
   appended beside `null_feature` before enumeration: its feature value may
   sort after ordinary features that have not yet been processed.
5. Start parallel ordinary-feature work only after the exclusion posting is
   available.

This keeps every segment feature ordered, gives the workers one shared
read-only exclusion posting, and avoids a special file or second merge
abstraction.

### Source Reads

The existing evidence supports treating the first implementation as
compute-bound:

- the long consolidation measurements are dominated by user CPU time;
- semantic posting transformation and compression are the common expensive
  work;
- the earlier Hazel read-ahead experiment improved elapsed time by only about
  one percent.

Use the direct, non-caching directory-position posting path initially. Do not
add reader threads or redesign source I/O before measuring the parallel merge.
The current `HazelFile` path serializes reads within one Hazel, so the plan
should not describe the data as memory-mapped or promise fully concurrent
reads from one source. Worker computation can still overlap reads, and
different source Hazels have independent read locks.

If the parallel implementation later stops scaling while CPUs wait for source
reads, the existing positioned-read machinery is the natural next experiment.
That is follow-up work, not part of the initial implementation.

### Recovery with Fewer Workers

For a recovery containing more segments than currently permitted workers:

1. Validate and scan every segment.
2. Treat the lowest-numbered permitted segments as active and all remaining
   segments as frozen.
3. Find the globally completed feature prefix.
4. Truncate active segments after that prefix as needed to restore an ordered
   append point. Recomputing discarded active-tail work is acceptable.
5. Leave frozen segments byte-for-byte unchanged.
6. Enumerate remaining source features in order, skipping features already
   completed in frozen segments, and append missing work only to active
   segments.
7. Include both active and frozen segments in the final k-way assembly.

This favors a simple, deterministic recovery rule over preserving every
possible completed active-tail record.

### Foreground Consolidation

`Bigwig::consolidate(...)` now distinguishes the new format from legacy state:

- discard legacy and invalid recovery files;
- retain a valid new `merge.*` group covering the consolidation target;
- allow the canonical activated `Hazel::merge(...)` procedure to resume it.

It also greedily groups consecutive Fivers by estimated size, merges and
converts those groups in parallel, and removes each source group after its
replacement Hazel is published.

## Clarifications Incorporated

- The serial `pst`/`dct` checkpoint construction was replaced by parallel
  posting-record logs.
- `null_feature` is a serial prerequisite; `text_chunk_tag` is precomputed but
  emitted at its sorted feature position.
- The implementation uses direct Hazel reads without claiming memory-mapped or
  fully concurrent reads from one source.
- Both `Bigwig::consolidate(...)` and background `merge_worker` resume coherent
  posting logs.
- The failure model is an append-only prefix with a possibly truncated tail;
  power-loss and storage-corruption guarantees are explicitly out of scope.

## Verification Additions

The verification list in `ai/consolidation.md` is appropriate. The following
cases are especially useful for the clarified design:

- a `text_chunk_tag` feature with ordinary features sorting on both sides of
  it, proving that serial precomputation does not break segment ordering;
- recovery where `null_feature` is complete and where its record is truncated;
- a valid new merge resumed across repeated Bigwig activations;
- an interrupted `finish-merging` run resuming new records while legacy
  `pst`/`dct`/`mrg` files are discarded;
- tail truncation inside the `PstRecord` header and each compressed payload;
- a structurally bad interior record causing the whole recovery group to be
  discarded;
- frozen segments containing completed features beyond a gap while active
  segments recompute their discarded tails;
- rejection of sequence-less or compressor-incompatible merge inputs.

## Deliberately Deferred

The initial implementation should not add:

- unique-source compressed-record copying during the semantic pass;
- dedicated source-reader threads;
- additional partition or metadata files;
- per-record durability calls or checksums;
- power-loss recovery guarantees;
- changes to posting, singleton, value, or exclusion semantics.

Measure CPU scaling, elapsed time, peak memory, and temporary disk use after
the canonical posting-log merge is working. Those results can determine
whether source I/O, raw-copy opportunities, or disk overlap deserve a later
design step.
