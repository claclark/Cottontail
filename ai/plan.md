# Goal: Review The Bigwig SimplePosting Merge-Cache Plan

Hazel, Fiver, and `SimplePosting` have been aligned for the next Bigwig cache
step:

- `SimplePosting` is storage only. It no longer manufactures hoppers and no
  longer inherits from `enable_shared_from_this`.
- `ArrayHopper` binds directly to waitable `SimplePosting` storage.
- Fiver and Hazel both expose concrete `posting(feature)` methods.
- Hazel idx cache entries are now waitable `SimplePosting`s, not `CacheRecord`s.
- `CacheRecord` remains for `SimpleIdx` and existing array-cache paths, but it
  is no longer part of Hazel's idx cache shape.

This plan records the next design checkpoint. It is not authorization to start
coding without discussion. Before editing implementation files, re-read this
plan with the user and confirm the exact scope.

## Design Direction

Bigwig should continue to expose ordinary hoppers to query code. Internally,
the feature mergepoint should return quickly when a real multi-shard merge is
needed:

1. create or find a cached waitable `SimplePosting`;
2. launch a background merge fill on a cache miss;
3. return `ArrayHopper::make(posting)` immediately.

This mirrors Hazel's current query-time cache behavior: a hopper can be handed
back before the backing posting has finished filling, and the hopper waits at
bind time.

The first cache step should remain Fiver-only. Bigwig's started view still
narrows live shards to `Fiver`, and Hazel is not yet part of the Bigwig live
query path. The point is to validate deferred Bigwig merged postings over the
existing Fiver view before adding mixed Fiver/Hazel shards.

The future direction is a Fluffle-managed shard interface implemented by both
Fiver and Hazel, with a virtual raw-posting operation matching the concrete
`posting(feature)` methods. Do not introduce that superclass in the immediate
cache diff unless the user explicitly widens the scope.

## Mergepoint Responsibilities

The feature-level mergepoint currently lives at:

```
Fiver::merge(fivers, feature, error, cache, ...)
```

For now it remains Fiver-only, but it should be shaped as the future mixed
Fiver/Hazel mergepoint:

1. Scan shards with fast `idx()->count(feature)`.
2. Return `EmptyHopper` if no shard contributes.
3. If one shard contributes, delegate to that shard's own `idx()->hopper`.
4. If multiple shards contribute, use the Bigwig-provided merge cache.

Direct one-shard delegation is important. It preserves Fiver's in-memory
behavior today and Hazel's lazy cache behavior later. Bigwig should stay mostly
as a query composer rather than accumulating shard-policy details.

## Bigwig Cache Shape

Keep the Bigwig cache conceptually simple:

```
feature -> std::shared_ptr<SimplePosting>
```

On a multi-shard cache miss:

1. Create a closed/deferred `SimplePosting` for the feature.
2. Insert it into the cache before launching work, so concurrent hopper
   requests share the same pending posting.
3. Launch a background thread that merges contributor postings into the cached
   posting.
4. Release the posting after the vectors are complete.
5. Return `ArrayHopper::make(posting)` to the caller.

On a cache hit, return `ArrayHopper::make(posting)` immediately. The hopper
will wait if the posting is still being filled.

Do not reintroduce a separate Bigwig `CacheRecord` completion latch unless a
concrete problem appears. `SimplePosting` already has the one-way completion
gate needed for this step.

## Merge Implementation

The current synchronous merge helper is
`SimplePostingFactory::posting_from_merge(...)`, which creates and returns a
complete `SimplePosting`.

For the deferred cache fill, add the narrowest helper that can fill an existing
destination `SimplePosting` without changing posting semantics. Preserve:

- the append fast path when lists are globally ordered and non-overlapping;
- the priority-queue merge behavior otherwise;
- the existing `push(...)` semantics;
- the existing separation between raw merging and deletion/exclusion handling.

The helper should consume Fiver `posting(feature)` results in the first step.
Later, the same shape can consume Hazel `posting(feature)` results through the
future shared shard interface.

## Deletion Semantics

Deletion behavior stays compositional:

```
NotContainedIn(
    merged_or_direct_hopper(feature),
    merged_or_direct_hopper(null_feature))
```

Do not physically apply exclusions while filling Bigwig's feature cache.
`null_feature` should go through the same merged/direct cache path as ordinary
features and remain reusable by the `NotContainedIn` query composition.

## Ownership And Lifetime

For the immediate step, Bigwig's merged-posting cache can remain owned by the
started `BigwigIdx`, as it is today. This keeps the implementation modest.

A separate cache-lifetime improvement remains open: preserve the Bigwig
merged-posting cache across `end()` -> `start()` when the logical sequence has
not changed. That is not a sequence-versioned Fluffle cache; it is one cache
generation that stays valid until a commit changes the logical sequence. Do not
add this incidentally without discussing ownership and invalidation first.

## Mixed Shard View

This is not part of the immediate cache step.

Later mixed Fiver/Hazel query support requires Bigwig's started view to retain
generic shard Warrens, or a new Fluffle shard superclass, rather than narrowing
all live entries to `Fiver`.

Review the txt path before that work:

- `BigwigTxt` currently stores `std::vector<std::shared_ptr<Fiver>>`;
- its translate/token/range behavior already uses public Warren txt methods;
- determine with the user whether changing it to generic Warrens or to the new
  shard interface is the right minimal mixed-view step.

Do not begin generic-Warren Bigwig view changes, background Hazel
materialization, activation preference, Fiver retention, or Hazel compaction
policy work incidentally.

## Discussion Checklist Before Coding

Confirm with the user:

- that the immediate implementation remains Fiver-only;
- whether the async cache fill belongs inside feature-level `Fiver::merge(...)`
  for now;
- the exact helper name for filling an existing `SimplePosting`;
- the thread-launch and failure behavior for Bigwig async cache fills;
- whether `Fiver::merge(...)` should keep using the caller-supplied `SafeMap`
  or move to a more explicit cache-entry helper;
- whether Bigwig's cache remains owned by the started `BigwigIdx`;
- whether focused compile-only verification is sufficient for the approved
  implementation step;
- whether `apps/trec-example` should be run by the user as the substantial
  dynamic regression after this cache step.

## Useful Starting Points

- `src/simple_posting.h`
- `src/simple_posting.cc`
- `src/array_hopper.h`
- `src/array_hopper.cc`
- `src/fiver.h`
- `src/fiver.cc`
- `src/bigwig.cc`
- `src/hazel.cc`
- `src/fluffle.h`
- `apps/trec-example.cc`

## Verification Rule

Agents should run compile/build checks only. Do not run test cases, including
`bazel test`, runtime experiments, ranking runs, evals, or benchmarks unless
the user explicitly asks for that specific verification work.
