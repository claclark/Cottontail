# Hazel File Format

This note describes the current Hazel v1 file produced by `Fiver::hazel(...)`.
It is intended as a guide for implementing Hazel activation.

Hazel is a single immutable shard file. The first producer is a live, built
Fiver under Bigwig control, but the file should be activated as a standalone
Hazel component pair: `hazel_idx` plus `hazel_txt`.

The current activation code is only a stub. `Warren::make(...)` can recognize
a Hazel file as a single-file burrow, read and dump its DNA, and construct an
inert Hazel Warren. The stub `HazelIdx` returns empty hoppers and zero counts;
the stub `HazelTxt` returns empty strings and zero tokens. Real idx/txt blob
activation is still pending.

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

The `idx` recipe records the posting and fvalue compressors. The `txt` recipe
records the text compressor and `chunk_size`.

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
absolute byte range; all offsets inside the txt blob are relative to the start
of that blob.

Header:

```
"COTTONTAIL_HAZEL_TXT_V1\n"
addr directory_offset
addr directory_length
addr directory_count
addr raw_text_length
addr target_chunk_size
```

The writer streams compressed text chunks first, keeps the text chunk directory
in memory, writes the directory at the end of the txt blob, then patches the
header.

Text directory entry:

```
addr raw_end
addr compressed_end
```

The text directory is also a boundary list. The first raw chunk starts at raw
offset `0`, and later raw chunks start at the previous entry's `raw_end`. The
first compressed chunk starts at the end of the txt blob header, and later
compressed chunks start at the previous entry's `compressed_end`.

`raw_end` is a byte offset into the original Fiver text blob. `compressed_end`
is relative to the start of the txt blob.

Text chunks are formed by walking a private hopper over the Fiver idx posting
list for `text_chunk_tag`. Adjacent Fiver text chunks are grouped until the raw
byte span reaches at least `target_chunk_size` when possible. The default
target is 64 KiB, but `Fiver::hazel(...)` accepts an explicit value and
`apps/fiver2hazel` exposes it as:

```
fiver2hazel --chunk-size bytes fiver...
```

The final chunk extends to `raw_text_length`.

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

## Activation Sketch

Opening a Hazel shard should roughly:

1. Read and verify `#COTTONTAIL`.
2. Read DNA until the blank line and parse it with `cook(...)`.
3. Verify `warren:"hazel"` and component recipes.
4. Read the top-level blob dictionary.
5. Locate `idx` and `txt` blob ranges.
6. Build `hazel_idx` from the idx blob header and directory.
7. Build `hazel_txt` from the txt blob header and directory, plus a reference
   to the activated `hazel_idx`.
8. Check tokenizer and featurizer compatibility with the outer Bigwig context.

The idx directory is small enough to load into memory for binary search. The
txt directory is also expected to be much smaller than the compressed text and
can be loaded into memory initially.

## Bigwig Integration Notes

Hazels produced from Fivers are expected to be materialized under Bigwig
control. The source Fiver should already be built, immutable, and started.

Later Bigwig activation can prefer `hazel.n.m` over `fiver.n.m` when both
exist for the same sequence range. This allows a conservative transition:
materialize a Hazel, keep the Fiver pickle until the Hazel is known good, then
eventually discard the Fiver pickle.

Hazel/Hazel merge is deferred. The v1 format is intended to make that later
step straightforward: merge posting directories/lists for idx, and concatenate
or rechunk compressed text blobs while preserving the `text_chunk_tag`
relationship.
