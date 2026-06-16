# Hazel File Format

This note describes the current Hazel v1 file produced by `Fiver::hazel(...)`,
the activation path, Hazel-to-Hazel merge, and the regression coverage that
protects the current behavior.

Hazel is a single immutable shard file. The first producer is a live, built
Fiver under Bigwig control, but the file should be activated as a standalone
Hazel component pair: `hazel_idx` plus `hazel_txt`.

`Warren::make(...)` can recognize a Hazel file as a single-file burrow, parse
its DNA and blob dictionary, and construct working Hazel idx/txt components.
Hazel activation now includes idx and txt caching work for threaded ranking
workloads.

## Single-File Burrow Envelope

The top-level file envelope is intended to be reusable beyond Hazel:

```
#COTTONTAIL
<dna>
<blank line>
<top-level blob dictionary>
<component blobs>
```

`#COTTONTAIL\n` is treated as fixed magic bytes. Once the magic matches, the
reader line-reads DNA until the blank separator line. The DNA is the semantic
dispatch point: its `warren` value determines which single-file Warren opener
should interpret the rest of the file.

The top-level blob dictionary is the physical manifest. It gives named byte
ranges in the file, but it should not know the meaning of component payloads.
Hazel is the first `warren` type using this envelope. Future static shard types
can reuse the same magic/DNA/blob structure and interpret their own component
blobs.

## Top Level

The file begins with readable identification and DNA:

```
#COTTONTAIL
<dna>
<blank line>
<top-level blob dictionary>
<component blobs>
```

The DNA is the semantic manifest. It is written with the existing recipe
format and includes:

- `warren:"hazel"`
- `featurizer:[ name:"...", recipe:"..." ]`
- `tokenizer:[ name:"...", recipe:"..." ]`
- `idx:[ name:"hazel", recipe:[ ... ] ]`
- `txt:[ name:"hazel", recipe:[ ... ] ]`
- `hazel:[ sequence_start:"...", sequence_end:"..." ]`
- optional `parameters:[ ... ]` copied from the owning Warren during conversion

The `idx` recipe records the posting and fvalue compressors. The `txt` recipe
records the text compressor and `chunk_size`. The optional `parameters` block is
currently used to preserve Meadowlark/legacy owner metadata, such as
`format:"meadowlark"` and default container settings, when a standalone Hazel is
created from a Fiver by `apps/fiver2hazel`.

After the blank line comes the top-level blob dictionary:

```
"COTTONTAIL_HAZEL_BLOBS\n"
addr blob_count
repeat blob_count:
  addr name_length
  char[name_length] name
  addr offset
  addr length
```

Top-level blob offsets are absolute file offsets. Current blob names are:

- `idx`
- `txt`

The writer reserves this dictionary near the front of the file, streams the
component blobs, then seeks back and patches the final byte ranges.

All binary integer fields are currently written as native `addr` values, and
floating values as native `fval` values. This is a v1 internal format, not yet
a portable cross-architecture interchange format.

## idx Blob

The idx blob is self-contained. The top-level dictionary gives the absolute
byte range of the blob; all offsets inside the idx blob are relative to the
start of that blob.

Header:

```
"COTTONTAIL_HAZEL_IDX\n"
addr directory_offset
addr directory_length
addr directory_count
```

The writer streams posting-list bytes first, keeps the posting directory in
memory, writes the directory at the end of the idx blob, then patches the
header.

Directory entry:

```
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
duplicating the `PstRecord::n` value for fast query planning.

If the inferred byte range is empty, the entry represents the common singleton
posting `<feature, p, p, 0>`, and `count_or_p` is `p`. Empty posting-list
ranges are therefore not legal zero-count lists; they are inline singleton
token postings. Singleton annotations with `p != q` or `v != 0` are written as
normal `SimplePosting` records.

Activation may initially decode non-empty ranges using the same
`SimplePosting` physical encoding, but this should be treated as the Hazel v1
wire encoding, not as a promise that all future Hazel producers have
`SimplePosting` objects internally.

## txt Blob

The txt blob is also self-contained. The top-level dictionary gives the
absolute byte range. Header fields are relative to the start of the txt blob
unless otherwise noted. Compressed chunk boundaries in the text directory are
relative to the start of chunk space, immediately after the txt header.

Header:

```
"COTTONTAIL_HAZEL_TXT\n"
addr directory_offset
addr directory_length
addr directory_count
addr raw_text_length
addr target_chunk_size
```

The writer reserves the header, records the start of chunk space, streams
compressed text chunks, keeps the text chunk directory in memory, writes the
directory at the end of chunk space, then patches the header. `directory_offset`
is relative to the start of chunk space, not the start of the txt blob.

Text directory entry:

```
addr raw_end
addr compressed_end
```

The text directory is also a boundary list. The first raw chunk starts at raw
offset `0`, and later raw chunks start at the previous entry's `raw_end`. The
first compressed chunk starts at chunk-space offset `0`, and later compressed
chunks start at the previous entry's `compressed_end`.

`raw_end` is a byte offset into the original Fiver text blob. `compressed_end`
is relative to the start of chunk space. For a non-empty txt blob, the final
`compressed_end` should equal `directory_offset`.

Text chunks are formed by walking a private hopper over the Fiver idx posting
list for `text_chunk_tag`. Adjacent Fiver text chunks are grouped until the raw
byte span reaches at least `target_chunk_size` when possible. The default
target is 64 KiB, but `Fiver::hazel(...)` accepts an explicit value and
`apps/fiver2hazel` exposes it for all live Fivers in a burrow as:

```
fiver2hazel [--chunk-size bytes] [--convert] [--merge] burrow
```

With no mode flags, `fiver2hazel` performs both conversion and merge. Existing
exact per-Fiver Hazels are reused during conversion, and intermediate Hazels
are preserved after merge.

When scanning the burrow, `fiver2hazel` only considers strict shard names of
the form `fiver.<number>.<number>` and `hazel.<number>.<number>`, ignoring
sidecar files such as restart checkpoint files.

The final chunk extends to `raw_text_length`.

An empty txt blob is represented by the fixed header only:

```
directory_offset = 0
directory_length = 0
directory_count = 0
raw_text_length = 0
```

`target_chunk_size` is still present and must be positive. Activation treats
this as a normal no-text shard.

## Relationship Between idx And txt

Hazel txt should not build its own token-to-byte index. As with `FiverTxt`,
text lookup depends on the idx posting list for `text_chunk_tag`.

That posting list maps:

```
token interval p,q -> raw text byte offset in v
```

Activation should use Hazel idx to obtain a hopper for `text_chunk_tag`.
For a `translate(p, q)` request, Hazel txt should:

1. Use a private or mutex-protected hopper over `text_chunk_tag`.
2. Find the Fiver-style raw byte offsets bracketing the requested token range.
3. Use the txt chunk directory to locate the compressed text chunk or chunks
   containing those raw byte offsets.
4. Decompress only the needed compressed chunks.
5. Use the tokenizer's `skip(...)` logic to trim to exact token boundaries.

This mirrors `FiverTxt::translate(...)`, but uses Hazel's compressed byte
store instead of Fiver's in-memory `std::string`.

Hoppers are stateful. Activation should not share a single hopper across
threads without synchronization. Prefer creating private hoppers for readers
when practical, or protect shared hopper state with a mutex as `FiverTxt` does.

## Hazel Merge

Detailed merge design and restart notes now live in
`ai/hazel-merge-notes.md`. This section is the current high-level map.

There are two `Hazel::merge(...)` surfaces in `src/hazel.cc` / `src/hazel.h`:

- `Hazel::merge(working, hazel_names, parameters, error)` is the compatibility
  adapter used by `apps/fiver2hazel --merge`. It parses the old named-shard
  inputs, activates each Hazel through `Warren::make(...)`, checks filename/DNA
  sequence agreement when sequence metadata is present, and delegates to the
  activated overload with a null parameter pointer.
- `Hazel::merge(hazels, dst, parameters, error)` is the real implementation.
  The caller supplies activated Hazel objects and a final destination path
  `dst`. Merge sidecars live beside `dst` with `mrg.`, `pst.`, and `dct.`
  prefixes.

The activated merge assembles a complete `mrg.<dst-name>`, publishes it by
hard-linking that temp file to `dst`, and then deletes associated sidecars.
Startup is idempotent: if `dst` already exists, the merge reports success after
deleting sidecars; if `mrg.<dst-name>` exists without `dst`, the temp assembly
is discarded. Transitional cleanup also removes old-style `dst.*` sidecars.

Only the idx merge is checkpointed. `HazelIdx::merge(...)` owns
`pst.<dst-name>` and `dct.<dst-name>`:

- `pst.<dst-name>` stores merged posting payload bytes.
- `dct.<dst-name>` stores three-`addr` directory records for completed output
  features.
- On restart, the dictionary is truncated to a record boundary, structurally
  validated as a prefix with increasing features and sane end offsets, clipped
  to entries covered by actual posting bytes, and then the posting file is
  truncated to the surviving final end offset.
- The merge then resumes with the first feature greater than the last surviving
  committed dictionary record.

That repair path assumes ordinary single-writer append-prefix behavior for the
sidecar files. It is meant to survive process death and interruption without
turning Hazel into its own filesystem journal. It does not try to defend
against hostile files, reused prefixes for different merges, or filesystems
that can expose stale unrelated data at the end of an append stream.

The idx merge decodes participating postings through the activated input
`HazelIdx` objects and writes a merged posting for each output feature. It
synthesizes `null_feature` as the deletion/exclusion posting before ordinary
feature merges, and synthesizes `text_chunk_tag` by copying input text-chunk
anchors while adding cumulative raw text lengths. The older unique-source
raw-copy optimization is not part of the current restartable path; the future
plan for restoring that case lives in `ai/improvements.md`.

`HazelTxt::merge(...)` has no durable checkpoint. It copies already-compressed
text chunks from the activated input `HazelTxt` objects into the final assembly
stream, builds a new cumulative txt directory, and patches the txt blob header.
The txt side is deterministic and short relative to the long idx posting merge.

As of 2026-06-13, repeated manual interruption of
`apps/fiver2hazel --merge a.meadow` followed by restart completed successfully:
the final Hazel size matched prior saved merged outputs and the MARCO dev-small
ranking profile matched earlier clean/resumed results. The concrete run record
is in `ai/hazel-progress.md`.

`Fiver::hazel(...)` also has the prerequisite separator change: before writing
a non-empty Fiver to Hazel, it appends a trailing newline if the Fiver text does
not already end in a separator. Hazel merge itself does not invent separator
bytes.

## Regression Coverage

Hazel merge has a dedicated regression target, `//test:hazel_test`, in
`test/hazel.cc`. The aggregate `//test:tests` target intentionally excludes
`hazel.cc` so the Hazel regression can be run independently.

The test builds a temporary no-merge Bigwig from three small text inputs, one
transaction per input file, producing one Fiver shard per file. The corpus
includes ordinary token features, line/file annotations, an inline singleton
feature, valued ordinal annotations, phrase/query cases across file boundaries,
and enough repeated text to exercise Hazel text chunking.

Each run then:

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
  matching phrases, and absent phrases.

The regression is run with multiple compressor/chunk profiles:

- null posting/fvalue/text compressors with 16-byte Hazel text chunks;
- real compressors (`post` postings, `zlib` fvalues/text) with 16-byte chunks;
- the deliberately awkward `bad` compressor for posting/fvalue/text with
  16-byte chunks;
- real compressors with the default 64 KiB chunk size.

The last agent-side verification was compile-only:

```
bazel build //test:hazel_test
```

The user then ran `bazel test //test:hazel_test` successfully on
2026-05-26. Future agents should continue to follow the repository rule: do not
run test cases themselves unless the user explicitly asks for that specific run.

## Activation Status

Opening a Hazel shard currently:

1. Read and verify `#COTTONTAIL`.
2. Read DNA until the blank line and parse it with `cook(...)`.
3. Verify `warren:"hazel"` and component recipes.
4. Read the top-level blob dictionary.
5. Locate `idx` and `txt` blob ranges.
6. Build `hazel_idx` from the idx blob header and directory.
7. Ask the activated `hazel_idx` for a private `text_chunk_tag` hopper, then
   build `hazel_txt` from the txt blob header, directory, and that hopper.

The idx directory is small enough to load into memory for binary search. The
txt directory is also expected to be much smaller than the compressed text and
can be loaded into memory initially.

The current `hazel_idx` implementation loads the idx directory at activation
and uses a CacheRecord-backed decoded posting cache for non-inline posting
blobs. Inline singletons are served directly from the directory. Cache fills use
a 16-reader `ReadGate`; `HazelIdx::cache(feature)` fills synchronously, while
normal hopper construction can trigger asynchronous fills.

The current `hazel_txt` implementation:

- builds its text compressor from the txt recipe keys `compressor` and
  `compressor_recipe`;
- loads the txt chunk directory into memory at activation;
- precomputes the managed token range from the supplied `text_chunk_tag`
  hopper;
- uses `ReadGate` for positioned compressed-chunk reads;
- keeps a no-eviction decompressed chunk cache parallel to the txt directory;
- protects the shared `text_chunk_tag` hopper with a mutex;
- translates by trimming requests to the managed token range, using
  `rho(...)`/`ohr(...)` anchors, decompressing the external chunks that cover
  the needed byte window, and applying at most two tokenizer skips to trim
  edges.

`translate(...)` is forgiving: impossible, inverted, out-of-range, or failed
read/decompression requests return the empty string because the `Txt` API has
no error channel. `HazelTxt::clone_()` is unsupported; Hazel Warren cloning is
a shallow Warren-level operation over shared immutable components.

A clone of a started Hazel Warren starts the clone as well. Focused regression
coverage checks that the clone remains readable after the source Hazel is
ended.

## Bigwig Integration Notes

The next agreed design discussion is narrower than full background lifecycle
integration. The larger destination is to let a started Bigwig query across a
carefully blended set of Fiver and Hazel shards. The immediate next step is
Fiver-only: introduce a `PostingIterator`-style raw posting-list path usable
with both `SimplePosting` and `CacheRecord`, then integrate it into Bigwig for
Fiver shards before Hazel participates in the live query path. The concrete idx
cache/hopper direction is recorded in `ai/plan.md`. That plan must be reviewed
with the user before implementation.

The key query-path direction is:

- preserve Hazel's shard-local decoded posting cache;
- let Bigwig asynchronously cache raw posting-list merges when multiple
  sub-indexes contribute to a feature;
- delegate directly to a sub-index hopper when only one sub-index contributes;
- retain the existing `NotContainedIn(feature, null_feature)` hopper
  composition for deletion semantics;
- avoid a feature-by-sequence Fluffle cache unless future usage demonstrates a
  clear need.

Background lifecycle policy remains a separate discussion. Open questions
still include:

- when Bigwig should create Hazel shards from newly committed Fivers;
- how long Fiver pickle shards should be retained after Hazel materialization;
- whether activation should prefer Hazel over Fiver when both cover the same
  live range;
- how Fluffle should represent and compact live mixed Hazel/Fiver ranges;
- how background Hazel materialization and merge should publish or fail safely.
