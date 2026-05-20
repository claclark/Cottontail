# Plan: Rebuild HazelTxt

## Status

Planning checkpoint. Do not continue by tuning the old HazelTxt code. The
current `HazelTxt` implementation has deliberately been stripped to stubs so it
can be rebuilt from the simple txt-blob model.

This plan is the next coding guide.

## Current Ground Rules

- Keep the components simple and direct.
- Work in `src/hazel.cc` for HazelTxt activation unless there is a clear reason
  to touch another file.
- The `Fiver::hazel` txt writer changes are good: txt directory offsets and
  compressed chunk ends are relative to chunk space after the txt header.
- `Hazel::clone_()` is a Warren-level compatibility shim. It shallow-copies
  Hazel components. Do not design around cloning `HazelTxt` or `HazelIdx`.
- `HazelTxt::clone_()` should remain unsupported: set an error and return
  `nullptr`.
- `raw_(p, q)` can simply call `translate(p, q)`.
- Keep JSON/internal `Txt::wrap` weirdness out of this round.

## File Format Model

HazelTxt is activated from:

- Hazel filename
- txt blob offset
- txt blob length
- tokenizer
- `text_chunk_tag` hopper

The txt blob header is at `txt_blob_offset` and contains:

```
"COTTONTAIL_HAZEL_TXT_V1\n"
addr directory_offset
addr directory_length
addr directory_count
addr raw_text_length
addr target_chunk_size
```

The fixed header ends at the start of chunk space:

```
chunk_space_start = txt_blob_offset + header_length
```

`directory_offset` is relative to `chunk_space_start`.

The chunk map lives at:

```
map_start = chunk_space_start + directory_offset
```

Each map entry is:

```
addr raw_byte_end
addr compressed_byte_end
```

For chunk `k`:

```
raw_byte_start =
    k == 0 ? 0 : map[k - 1].raw_byte_end
raw_byte_end =
    map[k].raw_byte_end

compressed_byte_start =
    k == 0 ? 0 : map[k - 1].compressed_byte_end
compressed_byte_end =
    map[k].compressed_byte_end
```

The actual file read for chunk `k` starts at:

```
chunk_space_start + compressed_byte_start
```

Use `byte` terminology in identifiers. These are byte offsets, not character
counts.

## HazelTxt Members

Likely private state:

```
std::string therecipe_;
std::shared_ptr<ReadGate> read_gate_;
addr chunk_space_start_;
addr raw_text_length_;
std::vector<ChunkMapEntry> map_;
std::unique_ptr<CacheEntry[]> cache_;
std::shared_ptr<Tokenizer> tokenizer_;
std::shared_ptr<Compressor> compressor_;
std::unique_ptr<Hopper> hopper_;
std::mutex hopper_lock_;
std::mutex cache_write_lock_;
addr token_start_;
addr token_end_;
```

Use sentinels for an empty token range:

```
token_start_ = maxfinity
token_end_ = maxfinity
```

No separate `has_tokens_` flag is needed.

`ChunkMapEntry` should hold the two cumulative ends:

```
addr raw_byte_end;
addr compressed_byte_end;
```

Cache design is parallel to `map_`. Do not implement eviction.

Conceptual cache entry:

```
std::unique_ptr<char[]> bytes;
std::atomic<bool> present;
```

Prefer an array for the cache rather than `std::vector<CacheEntry>`, because
entries contain atomics and owned buffers and are not naturally copyable or
movable. `std::unique_ptr<CacheEntry[]>` is a good fit because the cache size is
fixed once the map is loaded.

## Activation

`HazelTxt::make(...)` should be one conceptual activation operation. Do not add
a separate `load(...)` method unless the code truly needs it.

Steps:

1. Store recipe, tokenizer, and supplied hopper.
2. Create `ReadGate` from the filename.
3. Build the txt compressor from `recipe` inside HazelTxt.
4. Read the fixed txt header from `txt_blob_offset`.
5. Validate magic, nonnegative lengths/counts, positive `target_chunk_size`,
   and that the map byte range is inside the txt blob.
6. Set `chunk_space_start_ = txt_blob_offset + header_length`.
7. Allocate map storage for `directory_count`.
8. Read the entire map in one large read.
9. Decode and validate map entries:
   - raw byte ends are monotonic
   - compressed byte ends are monotonic
   - compressed byte ends are in `[0, directory_offset]`
   - final `compressed_byte_end == directory_offset` for non-empty text
   - final `raw_byte_end == raw_text_length`
10. Create the parallel cache with the same number of entries as the map.
11. Precompute the managed token range from the supplied hopper:
    - first token from `tau(minfinity + 1)`
    - final token from `uat(maxfinity - 1)`
    - if no token range exists, set both token sentinels to `maxfinity`

Use the hopper overload that preserves the annotation value bits as an `addr`.
The `text_chunk_tag` value is an encoded raw byte offset.

## Range And Token Count

`range_(p, q)` should be trivial:

```
if token_start_ == maxfinity:
    return false
*p = token_start_
*q = token_end_
return true
```

`tokens_()` should also be trivial:

```
if token_start_ == maxfinity:
    return 0
return token_end_ - token_start_ + 1
```

This assumes the text token range is contiguous.

## Cache Obtain Method

Keep all cache concurrency inside one private method, probably:

```
char *obtain(size_t k)
```

`obtain(k)` should always return the pointer stored in the cache, never a local
temporary buffer.

Flow:

1. If cache entry `k` is present, return `cache_[k].bytes.get()`.
2. Otherwise, read and decompress chunk `k` into a local buffer.
3. After decompression, check the cache entry again.
4. If it is now present, discard the local buffer and return the cache pointer.
5. If it is still absent, take the single cache write lock.
6. Check once more under the lock.
7. If still absent, move the local buffer into the cache and mark it present.
8. Return `cache_[k].bytes.get()`.

Duplicate read/decompress work is acceptable. Do not introduce in-flight
condition variables or clever coordination.

Use acquire/release ordering around the `present` flag so readers only observe
`bytes` after publication. The writer stores `bytes` before setting `present`.
Once present, a cache line is never modified or evicted.

If read or decompression fails, return `nullptr`; `translate` can return an
empty string because the `Txt` API has no error channel.

## Translate

`translate_(p, q)` should be mostly math and copying.

Always trim the requested token range to the managed token range before hopper
work:

```
if token_start_ == maxfinity:
    return ""
if p < token_start_:
    p = token_start_
if q > token_end_:
    q = token_end_
if p == maxfinity or q < p:
    return ""
```

This must handle calls such as:

- `translate(100, maxfinity)`
- `translate(minfinity, 100)`
- `translate(1000, -100)`
- ranges entirely before or after this HazelTxt

Then:

1. Lock only around hopper calls.
2. Use the `text_chunk_tag` hopper to get the internal token-to-byte anchors:
   - `rho(p)` gives `[p0, q0]` and the raw byte start for `p0`
   - `ohr(q)` gives `[p1, q1]` and the raw byte start for `p1`
3. Unlock the hopper.
4. If `rho(p)` returns no interval, or its interval starts after the requested
   range, return `""`.
5. Derive a covering byte range that is guaranteed to contain the requested
   tokens:

```
cover_byte_start = left_anchor_byte
cover_byte_end = q > q1 ? raw_text_length_ : right_anchor_byte
```

6. Obtain the external chunks covering this byte range. These chunks will also
   cover any edge bytes needed by tokenizer skipping.
7. Derive final byte bounds using the same semantics as `FiverTxt`:

```
byte_start = left_anchor_byte
if p0 < p:
    byte_start = skip_from(left_anchor_byte, p - p0)
else:
    p = p0

if q > q1:
    byte_end = raw_text_length_
else if q0 == q1:
    byte_end = skip_from(byte_start, q - p + 1)
else:
    byte_end = skip_from(right_anchor_byte, q - p1 + 1)
```

8. For edge trimming, first skip bytes from the external chunk start to the
   internal token anchor, then use `tokenizer_->skip(...)`.
9. Do at most two tokenizer skips:
   - one for the left edge
   - one for the right edge
10. Copy bytes from cached chunks intersecting the final byte range into the
   result.

For binary search, remember `byte_end` is exclusive. If the final byte range is
empty, return `""`. Otherwise the last chunk is found from `byte_end - 1`.

Important distinction:

- `text_chunk_tag` intervals are internal token-to-byte mapping chunks.
- txt map entries are external compressed chunks.
- One external chunk may contain multiple internal chunks.
- External chunk boundaries must not split internal mapping intervals. This is
  stronger than merely not splitting tokens and is what makes the edge skips
  one-chunk operations.

## Validation

After implementing the plan:

- Build `//apps:fiver2hazel` and a Hazel-opening binary such as
  `//apps:working`.
- Use phrase searching as an early smoke test because it exercises much of the
  token/text machinery.
- Use the user's `rank.sh` stress test later, after HazelTxt works again.
