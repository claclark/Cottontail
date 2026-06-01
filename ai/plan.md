# Goal: Review The Concrete Hazel-Into-Bigwig Idx Plan

Hazel writer, standalone activation, Hazel-to-Hazel merge, scratch conversion
utilities, and dedicated Hazel regression coverage are in place. The next
coding milestone is to let a started Bigwig query across a mixed set of Fiver
and Hazel shards while preserving lazy loading, caching, and query-level
parallelism.

This plan records a design discussion, not an authorization to start coding.
Before editing implementation files, re-read this plan with the user, discuss
the shape, resolve naming questions, and get explicit approval to implement it.
Do not treat the plan as a request to hack together a transitional V1. Make
changes only where the mixed-shard query path creates a clear need.

## Design Direction

Bigwig should continue to expose ordinary hoppers to query code. Internally,
`BigwigIdx::hopper_(feature)` should schedule asynchronous raw posting-list
merges when multiple sub-indexes contribute to a feature. This is useful for
multi-feature queries: requesting hoppers schedules independent feature loads
and gives available hardware cores work before traversal reaches each hopper.

Hazel keeps its existing immutable-shard cache because it avoids repeated disk
reads and decompression. Bigwig gains a cache for merged raw posting lists.
Avoid a feature-by-sequence Fluffle cache unless future usage provides a clear
need: versioning, eviction, and lifetime rules become complicated quickly.

A simpler cache-lifetime improvement remains worth discussing separately:
preserve a Bigwig merged-posting cache across an `end()` -> `start()` boundary
when the logical sequence has not changed. This is not a sequence-versioned
cache: one cache generation remains valid until a new commit changes the
logical sequence. Do not add this incidentally without agreeing on ownership
and invalidation with the user.

Deletion semantics stay as they are today:

```
NotContainedIn(
    merged_or_direct_hopper(feature),
    merged_or_direct_hopper(null_feature))
```

Do not physically apply exclusions while filling Bigwig's feature cache.
Keeping raw merging and deletion semantics separate preserves the current
behavior and leaves physical exclusion merging as an evidence-driven future
optimization.

## Internal Idx Hook

Add a low-level internal hook allowing composite idx implementations to request
efficient posting-list iteration from arbitrary sub-indexes.

The intended C++ shape is:

```
class Idx {
protected:
  static PostingIterator posting(std::shared_ptr<Idx> idx, addr feature) {
    return idx->posting_(feature);
  }

private:
  virtual PostingIterator posting_(addr feature) {
    return PostingIterator();
  }
};
```

The protected static bridge is intentional. It lets `BigwigIdx`, as an idx
implementation, collaborate with arbitrary idx backends without making this
low-level operation part of the public query API.

Names such as `PostingIterator`, `posting(...)`, and `posting_(...)` are
provisional. Discuss naming with the user before implementation.

Expected backend implementations:

- `FiverIdx::posting_(feature)` is essentially a lookup returning iteration
  over its existing `SimplePosting`.
- `HazelIdx::posting_(feature)` should reuse the existing `cache(feature)`
  method, which already locates the feature, creates or reuses a cache line,
  and synchronously fills it when needed for internal access.
- Backends without a clear need may retain the default empty result.

## Posting Iterator

Introduce a lean value-type iterator over an immutable posting list.

It should:

- construct from `std::shared_ptr<SimplePosting>`;
- construct from `std::shared_ptr<CacheRecord>`;
- retain the relevant backing owner;
- store raw pointers into posting, qosting, and optional fvalue storage;
- expose small inline operations for front/end access and advancement;
- wait on a `CacheRecord` before extracting its raw pointers;
- avoid heap allocation for the iterator itself.

The iterator exists to let mixed Fiver/Hazel lists participate in one merge
without copying Hazel cache arrays into `SimplePosting` vectors first.

## Bigwig Idx Cache

Change `BigwigIdx` from its current cache of completed `SimplePosting`s to a
cache whose entry contains:

```
std::shared_ptr<CacheRecord> completion;
std::shared_ptr<SimplePosting> posting;
```

For Bigwig, the `CacheRecord` is a completion latch. The associated
`SimplePosting` owns the dynamically built vectors. Once completion is
signalled, the `SimplePosting` must remain immutable.

The merged-or-direct helper used by `BigwigIdx::hopper_(feature)` should:

1. Scan sub-indexes using their fast `count(feature)` methods.
2. Return `EmptyHopper` immediately if no sub-index contains the feature.
3. Return the contributing sub-index's own hopper directly if exactly one
   sub-index contains the feature.
4. Otherwise acquire the Bigwig cache lock and look for an existing entry.
5. On a miss, create an unready `CacheRecord` plus an empty `SimplePosting`,
   insert the pair, and launch a background merge thread.
6. Release the cache lock before constructing the returned hopper.
7. Return an `ArrayHopper` backed by the completion latch and cached
   `SimplePosting`.

The direct one-sub-index path is important. For example, a feature present in
one Hazel can use `HazelIdx::hopper_(feature)` directly, preserving Hazel's own
lazy disk/decompression cache behavior. When deletion annotations exist, the
ordinary feature hopper is still composed with the raw `null_feature` hopper
through `NotContainedIn`; direct delegation does not bypass deletion semantics.

The cache should remain about raw posting merging. `null_feature` goes through
the same merged-or-direct mechanism and may be reused by the existing
`NotContainedIn` composition.

## ArrayHopper Extension

Keep the low-level hopping implementation centralized in `ArrayHopper`.

Clean up its existing ready-`SimplePosting` construction path:

- Add `friend class ArrayHopper;` to `SimplePosting`.
- Replace the current constructor/static `make(...)` arguments that manually
  pass `n`, postings, qostings, and fostings extracted by
  `SimplePosting::hopper()`.
- Let `ArrayHopper` extract the private vector size and raw pointers itself.

Add a second `ArrayHopper` construction path taking both:

```
std::shared_ptr<CacheRecord> completion
std::shared_ptr<SimplePosting> posting
```

This unusual pairing is intentional:

- wait for readiness using `completion`;
- after waking, bind `n_` and raw pointers from `posting`;
- then use the existing low-level hopping code.

An `ArrayHopper` is a sealed box until its own `ready_` state is established.
Operational methods already call `wait()` before hopping. Preserve a single
clear invariant: after `ArrayHopper::wait()` returns, its length and raw
pointers are valid and stable.

Do not convert `CacheRecord` arrays to vectors as part of this work. Existing
SimpleIdx and Hazel cache behavior can remain unchanged.

## Iterator Merge

Add a merge operation that consumes a collection of fast posting iterators and
fills a destination `SimplePosting`.

Preserve current posting semantics and the important append optimization:

- when lists are already globally ordered and non-overlapping, append them;
- otherwise use priority-queue merging and `push(...)` behavior equivalent to
  the existing `SimplePostingFactory::posting_from_merge(...)` path.

Raw merge inputs reported present by `count(feature)` cannot disappear during
this operation because exclusions are not physically applied. This avoids
inventing synthetic empty posting arrays for the Bigwig async cache path.

## Mixed Shard View

The idx changes require Bigwig's started view to retain generic shard Warrens
rather than narrowing all live entries to `Fiver`.

Review the corresponding txt path as part of implementation:

- `BigwigTxt` currently stores `std::vector<std::shared_ptr<Fiver>>`;
- its translate/token/range behavior already uses public Warren txt methods;
- determine with the user whether changing it to generic Warrens is sufficient
  for the intended mixed Fiver/Hazel view.

Do not begin background Hazel materialization, activation preference, Fiver
retention, or Hazel compaction policy work incidentally. Those remain separate
policy decisions unless the user explicitly brings them into the approved
implementation scope.

## Discussion Checklist Before Coding

Confirm with the user:

- final names for the iterator, idx hook, and Bigwig cache entry;
- whether the iterator merge belongs in `SimplePostingFactory` or another
  narrowly scoped location;
- the exact empty/default iterator representation;
- the thread-launch and failure behavior for Bigwig async cache fills;
- whether Bigwig's cache remains owned by the started `BigwigIdx`, or should
  survive `end()` -> `start()` while the logical sequence remains unchanged;
- the minimal generic-Warren txt change needed for mixed Fiver/Hazel views;
- whether focused compile-only verification is sufficient for the approved
  implementation step.

## Useful Starting Points

- `src/idx.h`
- `src/array_hopper.h`
- `src/array_hopper.cc`
- `src/simple_posting.h`
- `src/simple_posting.cc`
- `src/bigwig.cc`
- `src/fiver.cc`
- `src/hazel.cc`
- `src/fluffle.h`
- `ai/hazel.md`

## Verification Rule

Agents should run compile/build checks only. Do not run test cases, including
`bazel test`, runtime experiments, ranking runs, evals, or benchmarks unless
the user explicitly asks for that specific verification work.
