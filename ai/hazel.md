# Hazel Consolidated Notes

This is the durable Hazel reference after the Hazel/Bigwig integration sequence
completed on 2026-06-20. It folds together the old Hazel format note, merge
restart note, progress log, and finished plan checkpoint.

Hazel is an immutable single-file shard format. It can be opened as a
standalone Warren and can also participate as a visible Bigwig shard through
the shared `Owsla` interface.

## Current State

- Hazel v1 writing, standalone activation, Hazel-to-Hazel merge, startup
  sanitization, Bigwig activation, and merge-worker integration are in place.
- `Fiver` and `Hazel` both subclass `Owsla`. The common shard surface is
  `posting(feature)`, `estimated_size()`, `get_sequence(...)`, and
  `discard(...)`.
- Bigwig startup runs `sanitize(...)`, activates the sanitized Hazel prefix
  before the Fiver suffix, and stores the visible snapshot as
  `std::vector<std::shared_ptr<Owsla>>`.
- Bigwig read snapshots compose postings from visible `Owsla` children.
  Normal multi-shard posting merges are cached through `OwslaCache`;
  `text_chunk_tag` remains mergeable but uncached.
- `Fiver::hazel(...)` in working-directory form writes and activates an
  unstarted Hazel. The explicit-filename overload remains a bool-returning
  writer.
- `Hazel::merge(hazels, dst, ...)` writes and activates an unstarted output
  Hazel. The working/name adapter remains a bool-returning compatibility
  surface for command-line callers.
- Source shard deletion is lifecycle policy in `merge_worker`, not part of
  Fiver-to-Hazel conversion. After successful Fluffle publication, selected
  sources are discarded through `Owsla::discard(...)`.

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
ready posting. Normal hopper construction can fill asynchronously.

Hazel txt activation loads the text directory into memory, builds its text
compressor from the txt recipe keys `compressor` and `compressor_recipe`, uses
a 16-reader `ReadGate` for positioned compressed-chunk reads, keeps a
no-eviction decompressed chunk cache, and protects the shared
`text_chunk_tag` hopper with a mutex.

`HazelTxt::clone_()` is unsupported. Hazel Warren cloning is a shallow
Warren-level operation over shared immutable components. A clone of a started
Hazel Warren starts the clone as well, and regression coverage checks that the
clone remains readable after the source Hazel is ended.

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
  adapter used by `apps/fiver2hazel --merge`. It parses old named-shard inputs,
  activates each Hazel through `Warren::make(...)`, checks filename/DNA
  sequence agreement when sequence metadata is present, and delegates to the
  activated overload with a null parameter pointer.
- `Hazel::merge(hazels, dst, parameters, error)` is the real implementation.
  The caller supplies activated Hazel objects and a final destination path
  `dst`; on success it returns the activated but unstarted output Hazel.

The activated merge is interruptible and retryable under a simple
single-writer directory assumption. It assembles a complete `mrg.<dst-name>`,
publishes it by hard-linking that temp file to `dst`, and then deletes
associated sidecars. If `dst` already exists, merge reports success after
deleting associated intermediates. If `mrg.<dst-name>` exists without `dst`,
the temp assembly is discarded.

Only idx construction is durably checkpointed. Sidecars live beside `dst`:

- `mrg.<dst-name>` is the final assembly temp.
- `pst.<dst-name>` stores merged idx posting payload bytes.
- `dct.<dst-name>` stores three-`addr` Hazel posting directory records for
  completed output features.

On restart, `HazelIdx::merge(...)`:

1. Truncates `dct.<dst-name>` to a whole number of directory records.
2. Reads complete directory records.
3. Checks the actual `pst.<dst-name>` size.
4. Keeps the largest sane directory prefix with strictly increasing features,
   non-decreasing `end` offsets, and posting bytes covered by the actual
   checkpoint file.
5. Truncates `dct.<dst-name>` and `pst.<dst-name>` to that surviving prefix.
6. Resumes with the first feature greater than the last committed feature.

If there is no complete directory record, the posting checkpoint is treated as
empty. If interruption leaves posting bytes without a complete directory
record, those bytes are uncommitted and trimmed. If a directory record points
past present posting bytes, that record and later records are trimmed.

The idx merge:

- treats activated Hazel inputs as immutable read-only handles;
- requires at least two inputs;
- verifies compatible idx recipes and normalized Hazel DNA;
- processes `null_feature` first as the deletion/exclusion posting;
- synthesizes `text_chunk_tag` by adjusting input raw text byte anchors by
  cumulative text lengths;
- merges ordinary feature postings through `SimplePostingFactory`.

The current restartable path decodes participating postings and writes the
merged result. The earlier unique-source raw-copy optimization was removed
when the restartable checkpoint path replaced the old merge implementation.
Restoring a narrow checkpoint-aware version of that fast path is a possible
future optimization.

The txt merge has no durable checkpoint. `HazelTxt::merge(...)` copies
already-compressed text chunks from activated inputs into the output stream,
builds a new cumulative txt directory, and patches the txt blob header. It
checks that all activated `target_chunk_size_` values match.

Output parameter handling:

- If the shared parameter pointer passed to the activated overload is non-null,
  output DNA writes `parameters: freeze(*parameters)`.
- If that pointer is null, output DNA inherits the `parameters` package from
  the last input Hazel, or omits it if the last input has none.

Sequence metadata handling:

- If input Hazels have `hazel.sequence_start` and `hazel.sequence_end`, all
  inputs must have them.
- Input ranges must be contiguous in caller-supplied order.
- Output DNA writes the first input's `sequence_start` and the last input's
  `sequence_end`.
- If sequence metadata is absent, merge does not invent it.

## Sanitization

`Hazel::sanitize(...)` owns Hazel-side startup cleanup and restart inventory:

- strict parsing of live Hazel shard names;
- same-type contained-shard cleanup;
- old sidecar cleanup;
- private merge sidecar cleanup;
- production of logical restartable Hazel merge records.

Current Hazel merge sidecars use the `mrg.`, `pst.`, and `dct.` prefixes, for
example:

```text
mrg.hazel.<start>.<end>
pst.hazel.<start>.<end>
dct.hazel.<start>.<end>
```

The merge path still cleans old-style `hazel.<start>.<end>.*` sidecars as a
transition aid.

`Fiver::sanitize(...)` owns strict Fiver parsing, same-type contained-shard
cleanup, and `kitten*` removal. Generic `temp.*` cleanup lives in `Working`
construction so all Working users get the same startup cleanup behavior.

Bigwig's sanitizer coordinator combines Fiver and Hazel inventories into the
required visible shape:

```text
[ Hazel prefix ][ Fiver suffix ]
```

The final Hazel may shadow leading Fivers at the boundary; those Fivers are
deleted so the Hazel wins. Other mixed Hazel/Fiver overlap or out-of-order
Fiver-before-Hazel relationships are rejected.

Fluffle owns sanitized pending Hazel merge recovery records as
`hazel_merges`. Startup copies them from the sanitized inventory for future
consolidation-worker restart handling. Current policy records them but does
not yet schedule them ahead of new Hazel/Hazel merges.

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
- boundary Fiver-to-Hazel conversion.

It then claims the selected shards in `fluffle->merging`, optionally spawns a
friend worker, releases the lock, performs the operation, reacquires the lock,
publishes the replacement shard, and discards selected sources only after
successful publication.

Current policy order:

1. Compute `hazel_merge_okay(...)`.
2. If Hazel work is allowed, try boundary Fiver-to-Hazel conversion first.
3. Merge a run of small available Fivers, preserving old Fiver behavior.
4. Merge the smallest available adjacent Fiver/Fiver pair.
5. If Hazel work is allowed, merge the smallest available adjacent Hazel/Hazel
   pair.
6. Otherwise report no recommendation.

The boundary Fiver conversion rule converts the first Fiver after the Hazel
prefix if it is at least 128 MiB, or if there is at least one Hazel and it is
the only live Fiver.

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
- Bigwig activation of a Hazel prefix plus Fiver suffix;
- non-empty Hazel `estimated_size()` behavior.

The regression is built with multiple compressor/chunk profiles:

- null posting/fvalue/text compressors with 16-byte Hazel text chunks;
- real compressors (`post` postings, `zlib` fvalues/text) with 16-byte chunks;
- the deliberately awkward `bad` compressor for posting/fvalue/text with
  16-byte chunks;
- real compressors with the default 64 KiB chunk size.

Repository rule: agents should run compile/build checks only. Do not run test
cases, ranking runs, evals, or benchmarks unless the user explicitly asks for
that specific runtime work.

## Performance Shape

The old progress log recorded many historical measurements. Exact numbers are
not strict benchmarks: binaries changed, timers changed, the host was sometimes
noisy, and some runs were user-reported manual checks. The durable conclusion
is the performance shape by shard structure.

All current MARCO dev-small semantic checks after the Meadowlark
stemmer/tokenizer fixes preserved the expected profile:

- `MRR @10: 0.18975923272843034`;
- `QueriesRanked: 6980`;
- known fake-result topics `645252` and `970152` in the ranker output.

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

- Add or refine focused tests around `find_merge_action(...)` and merge-worker
  classification/publication paths.
- Revisit the Hazel work gate if the current shard-count approximation does
  not give the desired worker mix under heavy concurrency.
- Decide whether recovered `HazelMergeRecovery` records should be scheduled
  ahead of new Hazel/Hazel merges.
- Improve lower-level error propagation from `ready_()` paths so failures do
  not collapse into only `"Transaction cannot be commited."`
- Consider concurrent shard activation after `sanitize(...)` has established a
  deterministic inventory order.
- Consider a checkpoint-aware unique-source fast path inside restartable
  `HazelIdx::merge`.
