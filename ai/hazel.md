# Hazel Consolidated Notes

This is the durable Hazel reference after the Hazel/Bigwig integration,
restartable parallel-merge work, and 2026-08-02 cache-lifetime discussion. It
folds together the format, activation, recovery, consolidation, and performance
checkpoints.

Hazel is an immutable single-file shard format. It can be opened as a
standalone Warren and can also participate as a visible Bigwig shard through
the shared `Owsla` interface.

## Current State

- Hazel v1 writing, standalone activation, Hazel-to-Hazel merge, startup
  sanitization, Bigwig activation, and merge-worker integration are in place.
- `Fiver` and `Hazel` both subclass `Owsla`. The common shard surface is
  `posting(feature)`, `estimated_size()`, `get_sequence(...)`, and
  `discard(...)`.
- Bigwig startup runs `sanitize(...)`, activates the sanitized Fiver/Hazel
  sequence in range order, and stores the visible snapshot as
  `std::vector<std::shared_ptr<Owsla>>`. Sequence gaps are valid.
- Bigwig read snapshots compose postings from visible `Owsla` children.
  Normal multi-shard posting merges are cached through `OwslaCache`;
  `text_chunk_tag` remains mergeable but uncached.
- Hazel query caches currently retain decoded postings and decompressed text
  chunks without eviction. The deferred Warren-level memory-trimming direction
  and Hazel text-lifetime issue are recorded in `ai/memory.md`.
- `Fiver::hazel(...)` in working-directory form writes and activates an
  unstarted Hazel. The explicit-filename overload remains a bool-returning
  writer.
- `Hazel::merge(hazels, dst, ...)` writes and activates an unstarted output
  Hazel through the canonical restartable posting-log procedure. Background
  two-Hazel merges use one posting worker; many-Hazel consolidation uses the
  available worker budget. The working/name adapter remains a bool-returning
  compatibility surface.
- Source shard deletion is caller lifecycle policy, not part of
  Fiver-to-Hazel conversion or the activated Hazel merge. `merge_worker`
  discards selected objects after Fluffle publication; foreground consolidation
  removes source files only after each replacement is published.

## Single-File Envelope

Hazel uses the shared Cottontail single-file burrow envelope:

```text
#COTTONTAIL
<dna>
<blank line>
<top-level blob dictionary>
<component blobs>
```

`#COTTONTAIL\n` is fixed magic. After it matches, activation line-reads DNA
until the blank separator line. The DNA is the semantic dispatch point:
`warren:"hazel"` selects the Hazel opener.

The top-level blob dictionary is the physical manifest. It names byte ranges in
the file, but it does not know the meaning of component payloads. Hazel is the
first Warren type using this envelope; future static shard types can reuse the
same magic/DNA/blob structure.

The Hazel DNA includes:

- `warren:"hazel"`;
- `featurizer:[ name:"...", recipe:"..." ]`;
- `tokenizer:[ name:"...", recipe:"..." ]`;
- `idx:[ name:"hazel", recipe:[ ... ] ]`;
- `txt:[ name:"hazel", recipe:[ ... ] ]`;
- `hazel:[ sequence_start:"...", sequence_end:"..." ]`;
- optional `parameters:[ ... ]` copied from the owning Warren.

The optional `parameters` block preserves Meadowlark or legacy owner metadata
such as `format:"meadowlark"` and default container settings.

The top-level blob dictionary format is:

```text
"COTTONTAIL_HAZEL_BLOBS\n"
addr blob_count
repeat blob_count:
  addr name_length
  char[name_length] name
  addr offset
  addr length
```

Blob offsets are absolute file offsets. Current blob names are `idx` and `txt`.
The writer reserves the dictionary near the front of the file, streams the
component blobs, then seeks back and patches final byte ranges.

All binary integer fields are native `addr` values, and floating values are
native `fval` values. Hazel v1 is an internal format, not a portable
cross-architecture interchange format.

## Idx Blob

The idx blob is self-contained. The top-level dictionary gives the absolute
byte range of the blob; offsets inside the idx blob are relative to the start
of that blob.

Header:

```text
"COTTONTAIL_HAZEL_IDX\n"
addr directory_offset
addr directory_length
addr directory_count
```

The writer streams posting-list bytes first, keeps the posting directory in
memory, writes the directory at the end of the idx blob, then patches the
header.

Directory entry:

```text
addr feature
addr end
addr count_or_p
```

Directory entries are ordered by feature, matching the Fiver index map order.
Activation can binary-search by `feature`.

The directory is a boundary list, similar to `SimpleIdx`. For entry `i`, the
posting byte range begins at the previous entry's `end`, or at the end of the
idx blob header for the first entry. It ends at this entry's `end`.

If the inferred byte range is non-empty, it contains a posting list written
with `SimplePosting::write(...)`, using the posting and fvalue compressors
recorded in the Hazel DNA. In that case `count_or_p` is the posting count,
duplicating the `PstRecord::n` value for query planning.

If the inferred byte range is empty, the entry represents the common singleton
posting `<feature, p, p, 0>`, and `count_or_p` is `p`. Empty posting-list ranges
are therefore not legal zero-count lists; they are inline singleton token
postings. Singleton annotations with `p != q` or `v != 0` are written as normal
`SimplePosting` records.

## Txt Blob

The txt blob is also self-contained. The top-level dictionary gives the
absolute byte range. Header fields are relative to the start of the txt blob
unless otherwise noted. Compressed chunk boundaries in the text directory are
relative to chunk space, immediately after the txt header.

Header:

```text
"COTTONTAIL_HAZEL_TXT\n"
addr directory_offset
addr directory_length
addr directory_count
addr raw_text_length
addr target_chunk_size
```

The writer reserves the header, records the start of chunk space, streams
compressed text chunks, keeps the text chunk directory in memory, writes the
directory at the end of chunk space, then patches the header.
`directory_offset` is relative to the start of chunk space, not the start of
the txt blob.

Text directory entry:

```text
addr raw_end
addr compressed_end
```

The text directory is a boundary list. The first raw chunk starts at raw offset
`0`, and later raw chunks start at the previous entry's `raw_end`. The first
compressed chunk starts at chunk-space offset `0`, and later compressed chunks
start at the previous entry's `compressed_end`.

`raw_end` is a byte offset into the original Fiver text blob.
`compressed_end` is relative to chunk space. For a non-empty txt blob, the
final `compressed_end` should equal `directory_offset`.

Text chunks are formed by walking a private hopper over the Fiver idx posting
list for `text_chunk_tag`. Adjacent Fiver text chunks are grouped until the raw
byte span reaches at least `target_chunk_size` when possible. The default
target is 64 KiB.

An empty txt blob is represented by the fixed header only:

```text
directory_offset = 0
directory_length = 0
directory_count = 0
raw_text_length = 0
```

`target_chunk_size` is still present and must be positive. Activation treats
this as a normal no-text shard.

## Text Lookup

Hazel txt does not build its own token-to-byte index. As with `FiverTxt`, text
lookup depends on the idx posting list for `text_chunk_tag`.

That posting list maps:

```text
token interval p,q -> raw text byte offset in v
```

For a `translate(p, q)` request, Hazel txt:

1. Uses a mutex-protected hopper over `text_chunk_tag`.
2. Finds the Fiver-style raw byte offsets bracketing the requested token range.
3. Uses the txt chunk directory to locate compressed text chunks containing
   those raw byte offsets.
4. Decompresses only the needed compressed chunks.
5. Uses tokenizer `skip(...)` logic to trim to exact token boundaries.

`translate(...)` is forgiving: impossible, inverted, out-of-range, failed-read,
or failed-decompression requests return the empty string because the `Txt` API
has no error channel.

## Activation And Runtime

Opening a Hazel shard currently:

1. Reads and verifies `#COTTONTAIL`.
2. Reads DNA until the blank line and parses it with `cook(...)`.
3. Verifies `warren:"hazel"` and component recipes.
4. Reads the top-level blob dictionary.
5. Locates `idx` and `txt` blob ranges.
6. Builds `hazel_idx` from the idx blob header and directory.
7. Asks `hazel_idx` for a private `text_chunk_tag` hopper, then builds
   `hazel_txt` from the txt blob header, directory, and that hopper.

Hazel idx activation loads the idx directory into memory for binary search.
Non-inline posting hoppers use an `OwslaCache` of waitable `SimplePosting`
entries. The winning cache caller fills the posting from the compressed blob
using a 16-reader `ReadGate`; other callers wait on the same storage. The
concrete `Hazel::posting(feature)` method fills synchronously and returns a
ready posting. Normal hopper construction can fill asynchronously. Entries are
not evicted today. Because cache values and their consumers use shared posting
ownership, a future internally locked in-place clear can leave active hoppers
and fill workers valid; a query after the clear may perform a duplicate fill.

Hazel txt activation loads the text directory into memory, builds its text
compressor from the txt recipe keys `compressor` and `compressor_recipe`, uses
a 16-reader `ReadGate` for positioned compressed-chunk reads, keeps a
no-eviction decompressed chunk cache, and protects the shared
`text_chunk_tag` hopper with a mutex. The cache is a fixed array of lazily
populated raw chunk buffers. Translation copies required bytes into an owned
`std::string`, so no cached pointer escapes the operation. Concurrent eviction
is not safe as currently written: `obtain(...)` returns a raw pointer owned by
an entry's `unique_ptr<char[]>`, and `raw_bytes(...)` uses it after the cache
publication lock has been released. A clean lifetime or shared/exclusive
locking design remains deliberately unresolved.

`HazelTxt::clone_()` is unsupported. Hazel Warren cloning is a shallow
Warren-level operation over shared immutable components. A clone of a started
Hazel Warren starts the clone as well, and regression coverage checks that the
clone remains readable after the source Hazel is ended. The clone shares the
same `HazelIdx` and `HazelTxt` objects and therefore the same posting and text
caches; ending either Warren does not release those components while another
owner remains.

Hazel sequence metadata is optional for standalone files. Activation validates
it when present and caches `-1, -1` for `get_sequence(...)` when it is absent.

## Fiver-To-Hazel Conversion

`Fiver::hazel(...)` writes a Hazel from a live, built Fiver. Before writing a
non-empty Fiver to Hazel, it appends a trailing newline if the Fiver text does
not already end in a separator. Hazel merge itself does not invent separator
bytes.

The working-directory overload:

- chooses the default `hazel.<start>.<end>` name from the Fiver sequence range;
- writes through a temporary Hazel file;
- publishes by linking the temporary file to the final Hazel name;
- activates and returns the unstarted Hazel on success.

The explicit-filename overload remains a bool-returning writer. Source shard
discard is no longer part of conversion.

`apps/fiver2hazel` operates on burrow directories. It discovers strict shard
names of the form `fiver.<number>.<number>` and `hazel.<number>.<number>`,
ignoring sidecar files. It supports:

```text
fiver2hazel [--chunk-size bytes] [--convert] [--merge] burrow
```

With no mode flags, it performs both conversion and merge. Existing exact
per-Fiver Hazels are reused during conversion, and intermediate Hazels are
preserved after merge.

## Hazel Merge

There are two `Hazel::merge(...)` surfaces:

- `Hazel::merge(working, hazel_names, parameters, error)` is the compatibility
  adapter used by named-shard callers. It parses and activates each Hazel,
  validates filename/DNA sequence agreement and increasing non-overlapping
  ranges, chooses the combined destination name, and delegates. Gaps are
  allowed.
- `Hazel::merge(hazels, dst, parameters, error)` is the real implementation.
  The caller supplies activated Hazel objects and a final destination path
  `dst`; on success it returns the activated but unstarted output Hazel.

The activated merge is a two-pass, interruptible procedure under the normal
single-process-per-Warren assumption.

### Posting Logs

Restart files live beside the target Hazel:

```text
merge.<segment>.<sequence-start>.<sequence-end>
```

The segment number is zero-based and padded consistently for the segment
count. Each file is an append-only, feature-ordered sequence of complete
`SimplePosting` records. There is no checkpoint dictionary or separate merge
marker.

- Exactly two inputs use one posting worker.
- Three or more inputs use `allowed_threads(0)`, capped by available features.
- The initial contiguous segment set fixes the durable segment count.
  Recovery never widens it.
- If a later activation permits fewer workers, excess segments remain frozen
  but participate in completion tracking and final assembly.

The posting pass enumerates the union of the sorted source directories.
`null_feature` is merged serially and retained as immutable exclusion input.
The adjusted `text_chunk_tag` posting is also constructed before workers
start, but it is written only when its sorted feature position is claimed.

Workers dynamically claim features, read source postings through the
non-caching directory-position path, merge sources in chronological shard
order through `SimplePostingFactory`, apply exclusion semantics, and append one
complete posting record. An empty filtered result is written as an `n == 0`
completion marker.

`SimplePostingFactory::posting_from_merge(...)` supplies the annotation merge
semantics: innermost intervals survive; when intervals have identical
`(p, q)`, the value from the newest source survives. Fiver and Hazel callers
preserve oldest-to-newest shard order, so chronological merge grouping retains
those semantics.

### Recovery and Assembly

Recovery assumes every visible byte came from the segment's single sequential
writer, with only the final record potentially truncated. It does not attempt
power-loss, media-corruption, checksum, or hostile-filesystem recovery.

Each segment scan validates record fields, bounds, overflow, and strict feature
ordering. A partial trailing header or payload is truncated to the last
complete record. Structurally bad interior data invalidates the group.
Completed features from all segments are compared with the source feature
union. Active segments may be truncated to restore a common ordered append
point; frozen segments remain byte-for-byte unchanged.

After all features are complete, final assembly k-way scans the logs:

- `n == 0` completion markers are omitted;
- eligible plain singletons become normal inline Hazel directory entries;
- all other compressed posting records are copied into the final idx posting
  area without decompression or recompression.

`HazelTxt::merge(...)` continues to copy already-compressed text chunks, build
the cumulative txt directory, and require matching source chunk sizes. The
idx, txt, DNA, and blob dictionary are assembled in the ordinary
`dst + ".tmp"` file and then published as `dst`. Source Hazels and posting logs
remain until publication succeeds. If the target already exists, it wins and
its remaining logs are removed.

Output parameter handling:

- If the shared parameter pointer passed to the activated overload is non-null,
  output DNA writes `parameters: freeze(*parameters)`.
- If that pointer is null, output DNA inherits the `parameters` package from
  the last input Hazel, or omits it if the last input has none.

Sequence metadata handling:

- Merge inputs must have `hazel.sequence_start` and `hazel.sequence_end`.
- Input ranges must be increasing and non-overlapping in caller-supplied order;
  gaps are allowed.
- Output DNA writes the first input's `sequence_start` and the last input's
  `sequence_end`.

## Sanitization

`Hazel::sanitize(...)` owns Hazel-side startup cleanup and restart inventory:

- strict parsing of live Hazel shard names;
- same-type contained-shard cleanup;
- removal of legacy `mrg.*`, `pst.*`, `dct.*`, and old private sidecars;
- parsing and grouping of contiguous `merge.*` segment sets;
- production of `HazelMergeRecovery` records carrying target range, source
  Hazels, and segment count.

`Fiver::sanitize(...)` owns strict Fiver parsing, same-type contained-shard
cleanup, and `kitten*` removal. Generic `temp.*` cleanup lives in `Working`
construction so all Working users get the same startup cleanup behavior.

Bigwig's sanitizer coordinator combines Fivers and Hazels in sequence order.
Gaps from aborted commits are valid. A Fiver fully covered by a single Hazel is
deleted so the Hazel wins. A Fiver containing a Hazel, a partial mixed overlap,
or an unexplained same-type overlap is rejected.

A recovered merge is coherent only if its sources match a consecutive set of
living Hazels spanning its target. Invalid groups are deleted. If coherent
recoveries overlap each other, every conflicting group is deleted; startup
does not guess which one to keep.

Fluffle owns sanitized pending Hazel merge recovery records as
`hazel_merges`. Startup copies them from the sanitized inventory. Background
policy schedules recovered Hazel merges ahead of new Hazel/Hazel merges when
Hazel work is allowed. Foreground consolidation reuses a recovery only when it
matches the complete final Hazel input and deletes other partial merges.

## Bigwig Merge Worker

`merge_worker(...)` is organized around one policy function:

```cpp
find_merge_action(fluffle, &start, &end)
```

The policy recommends a visible shard range by index. The worker, while
holding the Fluffle lock, validates the recommendation, classifies it as one
of:

- Fiver/Fiver merge;
- Hazel/Hazel merge;
- Fiver-to-Hazel conversion.

It then claims the selected shards in `fluffle->merging`, optionally spawns a
friend worker, releases the lock, performs the operation, reacquires the lock,
publishes the replacement shard, and discards selected sources only after
successful publication.

Current policy order:

1. Apply Fiver policy:
   lone-Fiver cleanup; merge a run of at least three tiny eligible Fivers
   anywhere in the visible vector; convert the oldest eligible Fiver stranded
   between Hazels; convert the oldest eligible Fiver whose own estimate is at
   least `medium_shard`; merge the smallest adjacent eligible Fiver/Fiver pair
   where each side is below `medium_shard`.
2. If no Fiver action is available and Hazel work is allowed, continue a
   recovered Hazel merge first; otherwise merge the smallest eligible adjacent
   Hazel/Hazel pair.
3. Otherwise report no recommendation.

Current thresholds are `small_shard` = 8 MiB, `medium_shard` = 256 MiB, and
`large_shard` = 512 MiB. The Fiver policy is meant to sweep tiny update bursts
quickly, avoid producing tiny Hazels, and still make progress when updates
leave stranded or individually large Fivers.

The current Hazel work gate is intentionally simple: count Hazel shards already
in `fluffle->merging`; allow another Hazel-related action only when
`merging_hazels + 1 < fluffle->max_workers`.

Worker exits that observe no usable work or a terminal publication failure
decrement `fluffle->workers` while still holding the Fluffle lock. This avoids
the lost-worker race where `try_merge()` could see a retiring worker counted,
decline to spawn a replacement, and then have that worker exit.

## Regression Coverage

Hazel has a dedicated regression target, `//test:hazel_test`, in
`test/hazel.cc`. The aggregate `//test:tests` target intentionally excludes
`hazel.cc` so the Hazel regression can be run independently.

The test builds a temporary no-merge Bigwig from three small text inputs, one
transaction per input file, producing one Fiver shard per file. The corpus
includes ordinary token features, line/file annotations, an inline singleton
feature, valued ordinal annotations, phrase/query cases across file
boundaries, and enough repeated text to exercise Hazel text chunking.

Each run:

1. Converts each Fiver to a standalone Hazel with `Fiver::hazel(...)`.
2. Compares each Fiver shard against its matching Hazel.
3. Merges the Hazels with `Hazel::merge(...)`.
4. Moves the merged Hazel out of the Bigwig burrow so it opens as a standalone
   Hazel Warren.
5. Compares the source Bigwig against the merged Hazel.

The comparisons cover:

- `Txt::tokens()`, `Txt::range(...)`, and selected `Txt::translate(...)` spans;
- selected feature ids, counts, full posting streams, and absent-feature
  behavior;
- hopper probes through `L`, `R`, `tau`, `rho`, `uat`, and `ohr`;
- GCL queries including `line:`, `file:`, containment queries, term queries,
  matching phrases, and absent phrases;
- started Hazel clones that remain started and readable after the source Hazel
  is ended;
- Bigwig activation of mixed Fiver/Hazel sequences;
- non-empty Hazel `estimated_size()` behavior.

Focused recovery tests cover valid segment resume, incomplete groups,
aborted-transaction gaps, Fivers appearing inside a recovery range,
conflicting partial merges, legacy-file cleanup, and idempotent segment
removal.

The regression is built with multiple compressor/chunk profiles:

- null posting/fvalue/text compressors with 16-byte Hazel text chunks;
- real compressors (`post` postings, `zlib` fvalues/text) with 16-byte chunks;
- the deliberately awkward `bad` compressor for posting/fvalue/text with
  16-byte chunks;
- real compressors with the default 64 KiB chunk size.

Repository rule: agents should run compile/build checks only. Do not run test
cases, ranking runs, evals, or benchmarks unless the user explicitly asks for
that specific runtime work.

The user reports that the full `make testing` run passes at the 2026-07-26
commit checkpoint.

## Performance Shape

The old progress log recorded many historical measurements. Exact numbers are
not strict benchmarks: binaries changed, timers changed, the host was sometimes
noisy, and some runs were user-reported manual checks. The durable conclusion
is the performance shape by shard structure.

MARCO dev-small semantic checks after the Meadowlark stemmer/tokenizer fixes
preserve the expected profile:

- `MRR @10: 0.18975923272843034`;
- `QueriesRanked: 6980`;
- known fake-result topics `645252` and `970152` in the ranker output.

The 2026-07-04 user-run `rank.sh` checks used `bm25:b=0.68`,
`bm25:k1=0.82`, `bm25:depth=10`, `stop`, `stem`, and `bm25` over MARCO
dev-small:

| Burrow | Shape | Hot ranking loop | Wall time | Max RSS | MRR @10 |
| --- | --- | ---: | ---: | ---: | ---: |
| `a.meadow/` | Single merged Hazel | `12136 ms` | `0:13.73` | `5958648 KB` | `0.1897873743575748` |
| `b.meadow/` | One large Fiver | `6720 ms` | `1:23.37` | `21597860 KB` | `0.1896242666120888` |

Both ranked `6980` queries and emitted the known fake-result topics `645252`
and `970152`. The small MRR difference was inspected with a local diff of
rank triples: most differences were identical `(topic, docid)` pairs with only
rank positions changed, with a smaller number of top-10 boundary swaps. This
matches expected tie/order variation from highly parallel database build order.

On 2026-07-21, a user-run check of a newly built `a.meadow/` after the current
TSV and metadata changes reported `14863 ms` in the hot ranking loop,
`0:16.47` wall time, `6165964` KB max RSS, an MRR@10 of
`0.18971858370855488`, and `6980` ranked queries. It emitted the same two
known fake-result topics. The MRR lies within the previously observed
build-order variation and is not evidence of a semantic change.

At the 2026-07-26 checkpoint, independently built final Hazels reported
MRR@10 values `0.18948731068358557` and `0.1895927707281574`, each stable
across repeated runs. Comparing their saved top tens found 883 changed topics:
724 retained the same top-10 set in a different order, 159 changed at the
cutoff, and only 40 changed reciprocal rank. Those 40 changes exactly explain
the MRR delta. Ranking sorts by score without a document-id tie-break, so this
pattern is consistent with equal-score traversal-order churn between physical
merge layouts rather than a partial semantic failure.

Historical first-pass Hazel activation before decoded idx posting caching was
correct but unusably slow: around 43 minutes wall time and roughly 30 GB RSS
on the merged `a.meadow` Hazel. Adding the Hazel idx decoded posting cache
moved the same workload into the modern range: roughly 12 seconds internal
ranking-loop time, about 43 seconds wall time in the older `apps/working`
driver, and about 5.6 GB RSS.

Current representative ranges:

| Shape | Hot ranking loop | Wall time | Max RSS | Notes |
| --- | ---: | ---: | ---: | --- |
| Single merged Hazel | about 12.1-12.6 s | about 13.5-14.1 s in later `rank` runs | about 5.6-6.0 GB | Best end-to-end footprint for the large `a.meadow` check. |
| Three live Hazels | about 15.8 s | about 17.7-17.8 s | about 9.4-9.5 GB | Correct, but multi-Hazel posting composition is heavier than one merged Hazel. |
| Three large Fivers before `OwslaCache` | about 25.9 s | about 106 s | about 67.7 GB | Old baseline showing repeated concurrent merged-posting construction. |
| Three large Fivers after `OwslaCache` | about 9.1-9.4 s | about 73-80 s | about 21.8-23.9 GB | Much faster hot loop, still expensive to open and hold. |
| One large Fiver | about 6.6-7.0 s | about 74-82 s | about 21.6-21.8 GB | Fastest hot loop observed, but still high startup/RSS. |
| Mixed `[H,H,F]` | about 14.4 s | about 31 s | about 12.4 GB | Semantic validation point; performance was noisy. |
| Mixed `[H,F,F]` | about 11.3 s | about 64 s | about 18.8 GB | More Fiver-like memory/startup cost. |
| Mixed `[H,F]` after suffix consolidation | about 8.8 s | about 60 s | about 16.3 GB | Existing Fiver merge can consolidate the suffix while preserving the Hazel prefix. |

Interpretation:

- A single merged Hazel gives the best observed end-to-end behavior for large
  static shards: low RSS and fast startup, with a hot ranking loop around
  12 seconds on this workload.
- Large Fivers can be faster once hot, but they are expensive to open and hold
  in memory. The one-Fiver and post-cache multi-Fiver paths show why we do not
  want to throw Fiver behavior away.
- Multiple Hazels work correctly but pay multi-shard posting composition cost
  until background Hazel/Hazel merging collapses them.
- Mixed Hazel/Fiver Bigwig snapshots preserve query semantics. Their resource
  profile moves toward Fiver behavior as more suffix data remains in Fivers.

Maintenance timing observations:

- The completed foreground pipeline greedily grouped 29 consecutive Fivers,
  merged and converted the groups with 29 workers in 32.002 seconds, and then
  merged 37 Hazels in 260.583 seconds. Total consolidation was 317.915 seconds,
  down from roughly 14 minutes for the preceding serial Fiver-conversion
  pipeline. Source Fivers and Hazels were removed after their replacements
  were published.
- Converting the existing three very large `a.meadow` Fivers to per-Fiver
  Hazels took about 12.5 minutes total. This is a stress case; normal Bigwig
  lifecycle should convert bounded-size active Fivers, not old giant pickles.
- Clean Hazel/Hazel merge observations for the same large inputs ranged from
  about 500 to 707 seconds, depending on implementation point and interruption
  history.
- Repeated manual interruption and restart of `fiver2hazel --merge a.meadow`
  completed successfully. The final Hazel matched saved output size
  `3219314852` bytes and preserved ranking correctness.
- An async read-ahead experiment improved Hazel merge time by only about 1% on
  HDD and was not worth the complexity. The remaining merge cost is more likely
  posting decode/merge/recompression and special posting handling than simple
  sequential I/O.

## Follow-Ups

Likely next discussions, not standing authorization:

- Reconsider the Warren-level `trim_memory()` direction in `ai/memory.md` and
  choose a non-brutal concurrency/ownership design for evicting decompressed
  Hazel text chunks before implementing memory trimming.
- Add or refine focused tests around `find_merge_action(...)` and merge-worker
  classification/publication paths.
- Revisit the Hazel work gate if the current shard-count approximation does
  not give the desired worker mix under heavy concurrency.
- Improve lower-level error propagation from `ready_()` paths so failures do
  not collapse into only `"Transaction cannot be commited."`
- Consider concurrent shard activation after `sanitize(...)` has established a
  deterministic inventory order.
- Consider a posting-log-aware unique-source compressed-record fast path.
