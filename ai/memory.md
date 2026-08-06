# Memory Trimming Design Notes

These notes capture the 2026-08-02/03 discussion prompted by long-running
servers over indexes much larger than the original in-memory workloads. A
narrow first step now gives standalone Hazel servers a way to clear decoded
postings. The broader design remains a checkpoint rather than an active plan,
especially for Bigwig snapshots and Hazel text-cache mechanics.

## Motivation

Cottontail's query caches were designed mainly as lazy loading. The expected
workload activated an index, queried or updated it for a while, and ordinarily
fit the complete index comfortably in memory. Bigwig merged postings, decoded
Hazel postings, and decompressed Hazel text chunks therefore remain cached
without a general eviction mechanism.

Larger ClimbMix indexes are now being used by long-lived server processes,
including the server for the ClimbMix collection in TREC RAG 2026. Their memory
use can grow until the host swaps heavily or becomes unstable. Agent-driven
queries tolerate occasional large latency variation, so an aggressive memory
reduction is preferable to unbounded growth. A downstream fork carries a local
workaround, but crashes in additional Hazel servers made the narrow
posting-cache step urgent.

## Policy Boundary

Do not add Cottontail "server mode," an internal polling thread, or a global
memory-limit policy at this point. The surrounding service understands its
container, host, admission policy, and live Warren population better than the
library does.

A service can manage pressure as follows:

1. Detect its own high-water mark.
2. Delay admission of new queries.
3. Ask every active Warren, including live clones, to trim memory.
4. Allow in-flight users and pinned objects to drain, then measure again.
5. Resume below the service's chosen threshold or escalate by restarting,
   rejecting work, or taking another environment-specific action.

No Cottontail API for RSS, swap, or a memory limit is currently planned. If a
service measures Linux process footprint, resident memory plus attributed swap
is a better pressure signal than RSS alone, because swapping must not create
the illusion of new cache capacity. This measurement detail belongs to the
service policy rather than the Warren operation.

`Optimizer::estimate_memory(...)` gives a service a rough query preflight for
either a GCL string or parsed expression. It expands phrases, deduplicates term
features, and sums their index counts at three `addr` fields per posting. A
parse failure estimates zero because no hopper will be built. This estimates
decoded input-posting payload, not exact peak query memory: it excludes object
and allocator overhead, materialized results, and Warren-specific secondary
caches.

The current `ssr-server` applies a deliberately crude Linux-only policy at new
query admission. It reads `MemTotal` and `MemAvailable` from `/proc/meminfo`,
trims every persistent collection Warren when available RAM falls below one
third of total RAM, and rejects a query when the sum of its Warren estimates
exceeds one eighth of total RAM. Rejection uses the ordinary query error
response and creates no qid. Non-Linux builds and memory-information failures
skip both checks.

## Warren Operation

The public operation is:

```cpp
void Warren::trim_memory();
```

The name deliberately describes the caller's goal rather than a particular
cache representation. The operation should:

- be safe with concurrent query use;
- preserve query results, read snapshots, and durable state;
- release only state that can be reconstructed;
- be semantically harmless when repeated, including through several clones
  that share the same components;
- default to a no-op for Warren implementations with nothing safely
  reclaimable.

Trimming should be aggressive and substantial. Reducing currently retained
reconstructible memory by roughly half is an aspiration, not a strict API
guarantee and not a promise to halve total Warren or process memory. Immutable
structures, active hoppers, shared ownership, allocator retention, and other
application allocations all limit what one call can return to the operating
system. A `void` return avoids pretending that the library can report actual
process bytes reclaimed.

The initial implementation clears Hazel's decoded posting cache completely.
Other Warren implementations currently inherit the no-op default, and Hazel's
decompressed text cache is unchanged. The same API can later use LRU or
another selective policy without exposing that policy to callers. The disabled
SimpleIdx eviction experiment remains a separate question.

## Snapshot And Clone Ownership

The current Fluffle population is not the complete live query population.
Started Bigwig views capture both a vector of visible Owslas and a shared
Bigwig merged-posting cache generation. A clone of a started Bigwig shares that
same snapshot and cache. Commits and background merges may replace the current
Fluffle population while older started views continue to own and query their
historical Owslas and cache generation.

This is why process policy should call `trim_memory()` on every active Warren
it owns. Bigwig currently inherits the no-op default. A future Bigwig trim
should operate on both the current Fluffle state and the particular started
snapshot retained by that Bigwig. The likely mechanics are:

1. Briefly copy the current Fluffle cache and Owsla vector under
   `fluffle->lock`.
2. Briefly copy the Bigwig view's cache and Owsla vector under
   `warrens_lock_`, when that view is started.
3. Release both locks before clearing caches or recursively trimming Owslas.
4. Tolerate duplicate cache and Owsla references; repeated trimming is
   intentionally harmless.

Copying shared pointers keeps an Owsla alive while it is trimmed even if a
concurrent merge removes it from the current Fluffle. An older Owsla that is
already absent from the Fluffle remains reachable through the started Bigwig
view that retained it.

## Posting Caches

`OwslaCache` is shared by Bigwig posting composition and Hazel decoded
postings. It currently owns a mutex-protected map from feature to a waitable
`shared_ptr<SimplePosting>`.

`OwslaCache::clear()` is the internally locked in-place primitive. Clearing the
map is safe because hoppers and fill workers retain their own shared posting
ownership. An in-flight fill may complete after its entry was cleared, and a
later query may start a duplicate fill; this costs time and may temporarily
duplicate memory but does not change semantics. Clearing the same cache through
several clones is harmless. Hazel's `trim_memory()` uses this primitive now.

In-place clearing matters. Replacing only one cache pointer would not reach
`BigwigIdx`, Hazel components, clones, or older snapshots that already hold a
shared pointer to the original cache object.

## Hazel Text Cache

Hazel text is stored as compressed chunks with a directory of compressed and
raw byte boundaries. `HazelTxt` lazily reads and decompresses a chunk on first
translation, then retains its raw bytes in a fixed cache-entry array. Entries
are never evicted today.

Hazel Warren clones share the same `HazelIdx` and `HazelTxt` objects, so they
also share both caches. Ending one clone does not release those shared
components. Cached chunks live until every owning Hazel Warren is destroyed.

The raw chunk pointer is confined to translation: `raw_bytes(...)` copies the
needed bytes into an owned `std::string`, and no pointer or view escapes to the
caller. Concurrent eviction is nevertheless unsafe in the current
implementation because each entry owns a `unique_ptr<char[]>`, `obtain(...)`
returns its raw pointer, and the copy occurs after the cache's publication lock
has been released. Resetting the entry at that moment would cause a
use-after-free.

A shared/exclusive cache-lifetime lock is one plausible clean solution:
translations hold shared access while copying chunks, and trimming takes
exclusive access while releasing buffers. Other ownership designs may be
better. Do not implement a brute-force HazelTxt workaround until this has been
reconsidered.

## Fiver And Simple

A Fiver keeps much of its useful index and text state directly in memory; that
state is not automatically a disposable cache. Its initial `trim_memory()` may
therefore be a no-op unless a clearly reconstructible component is identified.

SimpleWarren and SimpleIdx are outside the initial direction. The old disabled
SimpleIdx eviction policy and possible future LRU work remain documented in
`ai/improvements.md` and should not be silently folded into this design.

## Deferred Work

- Choose a clean concurrency and ownership mechanism for Hazel text chunks.
- Design and implement Bigwig trimming across current and historical snapshots.
- Decide whether future trims clear all optional state or implement an
  explicitly substantial partial reduction.
- Add focused concurrency coverage for in-flight posting fills and historical
  Bigwig snapshots. Current Hazel coverage checks shared clones, active hoppers,
  cache refill, and repeated trims.
