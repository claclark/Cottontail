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
"COTTONTAIL_HAZEL_BLOBS_V1\n"
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
"COTTONTAIL_HAZEL_IDX_V1\n"
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
"COTTONTAIL_HAZEL_TXT_V1\n"
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
fiver2hazel [--chunk-size bytes] [--force] burrow
```

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

`Hazel::merge(...)` has a Working-based implementation in `src/hazel.cc`,
declared in `src/hazel.h`:

```
Hazel::merge(working, hazels, parameters, error)
```

The merge writes a complete temp file from `working->make_temp("hazel")`,
publishes it with a hard link to the canonical final shard name, then removes
the temp file. The canonical output name is:

```
hazel.<first_sequence_start>.<last_sequence_end>
```

The implementation is structured around two local state structs:

- `HazelMergeInput`: one validated input Hazel, opened once, with parsed DNA,
  blob dictionary, idx/txt directories, sequence range, txt chunk metadata, and
  a `front` cursor over the sorted idx directory.
- `HazelMergeOutput`: the output stream plus blob dictionary state and helper
  methods for writing/patching txt and idx blobs.

Input validation currently checks:

- each input name matches `hazel.<sequence_start>.<sequence_end>`;
- input ranges are contiguous in the user-supplied order;
- the internal `hazel.sequence_start` / `hazel.sequence_end` DNA values exactly
  match the filename values;
- all input DNA is compatible after ignoring only
  `hazel.sequence_start`, `hazel.sequence_end`, `parameters`, and txt
  `chunk_size`;
- idx/txt blob headers and directories are internally consistent.

The txt merge copies compressed chunks without decompressing or recompressing.
Each input chunk is read once from its already-open source file and written
unchanged into the output txt chunk space. Output txt directory entries use new
cumulative raw and compressed byte ends. Output `raw_text_length` is the sum of
input raw text lengths, and output `target_chunk_size` is the minimum input
chunk size.

The idx merge treats each input idx directory as a sorted feature stream. The
stream front is a directory cursor, not an eagerly decoded posting. The sweep
finds the next feature by inspecting front feature ids, decodes only
participating postings, writes the merged output posting, and advances those
inputs. Inline singleton entries are decoded into one-record `SimplePosting`s
before merge and are written back inline only when the final posting is exactly
one `<p, p, 0>` record.

Special postings are synthesized before the ordinary sweep and then slipped
into the output stream at their natural feature positions:

- `null_feature` is merged into `exclude` and used as the exclusion list for
  ordinary feature merges.
- `text_chunk_tag` is synthesized by copying input token intervals and adding
  cumulative prior raw text length to each value.

When the sweep reaches `null_feature` or `text_chunk_tag`, raw input entries
for those features are consumed and skipped because their contributions were
already folded into the synthesized posting.

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
7. Build `hazel_txt` from the txt blob header and directory, plus a reference
   to the activated `hazel_idx`.

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
currently a shallow compatibility shim over shared components.

Hazel supports shallow Warren cloning. This is sufficient for current
activation, ranking, and regression use, but the clone/caching model should be
kept in mind during Bigwig integration.

## Bigwig Integration Notes

Hazels produced from Fivers are expected to be materialized under Bigwig
control. The source Fiver should already be built, immutable, and started.

Later Bigwig activation can prefer `hazel.n.m` over `fiver.n.m` when both
exist for the same sequence range. This allows a conservative transition:
materialize a Hazel, keep the Fiver pickle until the Hazel is known good, then
eventually discard the Fiver pickle.

Hazel/Hazel merge now has a standalone implementation as `Hazel::merge(...)`;
see `Hazel Merge` above. Bigwig/Fluffle does not yet call it for background
compaction. That integration is the next design step: Bigwig should eventually
choose Hazel shard names for compatible ranges, invoke the Working-based merge
primitive, and publish merged Hazel shards without disturbing existing inputs on
failure.

Open integration questions include:

- when Bigwig should create Hazel shards from newly committed Fivers;
- how long Fiver pickle shards should be retained while Hazel is proving itself;
- whether activation should prefer Hazel over Fiver when both cover the same
  live range;
- how background merge selection should represent live Hazel ranges;
- whether merged Hazel publication should rename/move the final shard out of a
  staging location or leave `Hazel::merge(...)` responsible for canonical
  publication;
- how Bigwig should handle mixed Fiver/Hazel states during an incremental
  transition.
