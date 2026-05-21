# Goal: Implement HazelIdx Cache Loading

Do not start coding without discussin and confirming with user.

Next coding step: add the smallest useful decoded posting cache to `HazelIdx`,
while keeping the design compatible with future Bigwig-level merged posting
caches.

The current Hazel ranker regression is correct but slow: the post-HazelTxt
rebuild run still spends about 43 minutes on the Hazel shard. The remaining
large bottleneck appears to be repeated HazelIdx posting reads, decompression,
and decoding.

## Design

Use the existing `CacheRecord` / `ArrayHopper` infrastructure from
`src/array_hopper.h`.

`HazelIdx` should own:

- the in-memory Hazel posting directory;
- one `ReadGate` for the Hazel file, initially with 4 readers;
- a feature-to-`std::shared_ptr<CacheRecord>` cache map;
- a cache-map mutex;
- one `SimplePostingFactory`.

`HazelIdx` should no longer need to keep posting/fvalue compressors directly
once the factory is constructed. The posting format details should remain in
`SimplePostingFactory`, because Hazel v1 non-inline posting blobs currently use
the `SimplePosting` physical format and future format/version compatibility
should live at that layer.

Add a public Hazel-specific cache accessor:

```c++
std::shared_ptr<CacheRecord> cache(addr feature);
```

This is not part of the generic `Idx` interface. It exists so future Bigwig
Hazel integration can ask a Hazel shard for decoded posting storage directly.

`cache(feature)` behavior:

- feature absent: return `nullptr`;
- inline singleton directory entry: return a freshly created ready
  `CacheRecord`, but do not insert it into the cache map;
- non-empty posting range:
  - check the cache map;
  - if present, return the existing entry, whether or not it is ready;
  - if absent, take the cache lock and check again;
  - if still absent, create and insert the entry, release the lock, then fill
    it synchronously on the caller's thread;
  - the caller fills exactly because it created the entry.

`hopper(feature)` behavior:

- feature absent: return `EmptyHopper`;
- inline singleton directory entry: return `SingletonHopper(p, p, 0.0)`
  directly;
- non-empty posting range:
  - use the same cache lookup/create rule as `cache(feature)`;
  - if this call creates the entry, launch an asynchronous fill;
  - return `ArrayHopper::make(entry)` immediately.

Thus normal Hazel `hopper(...)` calls preserve the existing SimpleIdx-style
behavior: query construction can create hoppers and start cache loads without
waiting; the first `tau(...)`, `rho(...)`, `uat(...)`, `ohr(...)`, `L(...)`, or
`R(...)` waits only if the entry is still loading.

## Fill Helper

Implement cache filling as a helper in the unnamed namespace rather than as a
method that captures `this`.

The helper should take all state it needs by value:

```c++
void fill_hazel_cache_entry(std::shared_ptr<CacheRecord> entry,
                            std::shared_ptr<ReadGate> read_gate,
                            std::shared_ptr<SimplePostingFactory> factory,
                            addr offset,
                            addr length);
```

The asynchronous path should capture only those shared pointers and scalar byte
ranges, avoiding detached-thread lifetime hazards.

The helper:

1. reads the compressed posting bytes through the Hazel `ReadGate`;
2. decodes directly into the supplied `CacheRecord`;
3. marks the record ready and notifies waiters on all paths.

Keep the asynchronous launch behind a tiny helper, conceptually
`async(work)`. For this Linux-first step it can use a detached `std::thread`.
The wrapper keeps a later macOS/thread-pool/GCD port localized.

## Posting Factory Work

Add a `SimplePostingFactory` method that decodes the existing compressed
posting blob format directly into a `CacheRecord`, avoiding construction of a
temporary `SimplePosting` only to convert it.

Shape the API around filling the existing shared entry, because Hazel inserts
the entry before loading:

```c++
bool fill_cache_record_from_compressed_blob(
    std::shared_ptr<CacheRecord> entry,
    const char *data,
    addr length,
    std::string *error = nullptr);
```

The method should set:

- `entry->postings`;
- `entry->qostings`, using the same storage as `postings` when there is no
  separate q array;
- `entry->fostings`, or `nullptr`;
- `entry->ready` only after the arrays are valid.

`entry->n` should already be initialized by Hazel from the directory
`count_or_p` before the hopper is created.

## ArrayHopper Adjustment

Change only the cache-line constructor assertion in `ArrayHopper` from
`n_ >= 2` to `n_ >= 1`.

Do not change the direct-array or `SimplePosting` constructors yet. The intent
is only to allow cache-backed singleton posting lists to wait lazily and then
operate as normal arrays. Inline Hazel singletons still bypass the cache and use
`SingletonHopper` directly.

## Deep Error Behavior

A failed Hazel posting read/decode is a deep format or storage error. For now:

- assert;
- still fill the entry with structured bogus intervals;
- mark it ready and notify waiters.

Use arrays of the originally promised `entry->n`. Keep them sorted and
non-null. A reasonable fallback is small low-boundary intervals such as:

```text
p[0] = minfinity + 1, q[0] = minfinity + 2
p[1] = minfinity + 2, q[1] = minfinity + 3
...
```

This should look strange if it leaks into results but should not hang waiters
or crash `ArrayHopper`. Later, when deep-error logging exists, log this path
even when assertions are disabled.

## Bigwig Direction

This Hazel-local cache is intentionally not the final Bigwig cache.

Future Bigwig Hazel integration should keep its own feature-to-merged-posting
cache. On a Bigwig `hopper(feature)` miss, Bigwig can create a merged
`CacheRecord`, launch one loader, and have that loader gather child postings
from Hazels and Fivers. When the loader reaches a Hazel, it calls
`HazelIdx::cache(feature)`: if the Hazel entry is missing, the Bigwig loader
borrows its own thread to fill it; if it already exists, Bigwig keeps the
shared pointer and waits when needed.

This avoids spawning a secondary herd of Hazel loader threads underneath a
Bigwig merge loader, while preserving standalone Hazel behavior.

## Not In This Step

- No eviction. Shared-pointer ownership makes future eviction straightforward:
  remove an entry from the map under the cache lock; existing hoppers or merge
  work keep their references alive.
- No Bigwig implementation yet.
- No platform-specific thread pool yet.
- No broad Hazel format changes.
