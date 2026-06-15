# Goal: Add Hazel Shards To Bigwig Read Snapshots Without Merging

Hazel, Fiver, and Bigwig have been aligned enough to take the next small
integration step:

- `SimplePosting` is storage only. It no longer manufactures hoppers.
- `ArrayHopper` binds directly to waitable `SimplePosting` storage.
- Fiver and Hazel both expose concrete `posting(feature)` methods.
- Hazel idx cache entries are waitable `SimplePosting`s, not `CacheRecord`s.
- Fluffle owns the Bigwig merged-posting cache generation.
- A Bigwig start captures both the visible shard snapshot and the current cache
  generation.
- The Fluffle cache generation is replaced when a kitten is committed into a
  visible Fiver, not when the write is merely prepared.
- `ArrayHopper` has explicit singleton and singleton-interval coverage; Fiver
  also restores its in-memory singleton fast path.

This plan records the next design checkpoint. It is not authorization to start
coding without discussion.

## Immediate Goal

Make Bigwig read snapshots include existing Hazel shards as read-only shards,
without introducing Hazel merge policy or Hazel/Fiver physical merge behavior.

The first mixed view should answer queries over whatever committed Fiver and
Hazel shards are already present in the Fluffle. It should not:

- convert Fivers to Hazels in the background;
- merge Hazels together;
- merge Fivers and Hazels into a new physical shard;
- choose retention or activation policy;
- introduce the future shard superclass unless the code clearly demands it.

Think of this as "Hazel is query-visible" rather than "Hazel participates in
the lifecycle machinery."

## Snapshot And Cache Invariant

The Bigwig feature cache is valid for one logical visible shard snapshot.

Visible means the shards that `Bigwig::start_()` will include in its read view.
Today this is Fiver-only. In the next step it should include Fiver and Hazel
shards.

The cache generation must change when the logical visible snapshot changes:

- a kitten being appended to Fluffle does not change the visible snapshot;
- a kitten becoming a committed Fiver does change the visible snapshot;
- adding an already-built Hazel to the visible Fluffle view changes the visible
  snapshot;
- background consolidation is supposed to preserve query answers. If it changes
  results, that is a consolidation bug, not a cache policy feature.

Do not move cache invalidation back into `ready()`. That was the source of a
stale-cache race in `apps/trec-example`: an old visible snapshot could populate
the new cache generation while the new Fiver was still a kitten.

## Bigwig Read View

Current Bigwig read state narrows to:

```
std::vector<std::shared_ptr<Fiver>>
```

The no-merge Hazel step should change the started view to hold generic readable
shards. The likely minimal type is:

```
std::vector<std::shared_ptr<Warren>>
```

This is intentionally less ambitious than a new Fluffle shard superclass. A
superclass may still be the right later cleanup, especially around
`posting(feature)`, but it is not required for simply querying already-started
Fiver and Hazel Warrens.

## BigwigTxt

`BigwigTxt` is the easiest part. It currently stores Fivers, but its operations
already use the public Warren txt interface:

- `warren->txt()->translate(p, q)`
- `warren->txt()->tokens()`
- `warren->txt()->range(...)`

The no-merge Hazel step should make `BigwigTxt` store generic Warrens and keep
that behavior. Do not add text merge logic.

## BigwigIdx

`BigwigIdx` currently calls `Fiver::merge(...)` over the Fiver-only view.

For the no-merge Hazel step, BigwigIdx should produce hoppers by composing shard
hoppers, not by physically merging postings:

1. Scan the started shard view with fast `idx()->count(feature)`.
2. Return `EmptyHopper` when no shard contributes.
3. If one shard contributes, delegate to that shard's own `idx()->hopper`.
4. If multiple shards contribute, return a `VectorHopper` over each
   contributor's `idx()->hopper(feature)`.

This makes Hazel query-visible without requiring the future mixed
`posting(feature)` mergepoint.

The existing multi-Fiver physical merge cache remains useful for the Fiver-only
path and later cache work, but it should not be forced into the first Hazel
visibility step. If the mixed view contains any Hazel, prefer no-merge hopper
composition for that feature.

## Deletion Semantics

Deletion behavior should remain compositional:

```
NotContainedIn(feature_hopper, null_feature_hopper)
```

For the mixed no-merge step, both `feature_hopper` and `null_feature_hopper`
can be constructed with the same shard-hopper composition rules:

- empty if no shard contributes;
- direct delegation if one shard contributes;
- `VectorHopper` when multiple shards contribute.

Do not physically apply exclusions while composing a mixed Fiver/Hazel query
view.

## Activation

Bigwig activation currently discovers live Fiver files and unpickles them into
Fluffle. The next step should additionally discover already-existing Hazel shard
files and activate them into the same Fluffle read list.

Keep the first policy conservative:

- only strict shard filenames should be considered;
- do not let sidecar files or partial checkpoint files become visible;
- preserve sequence ordering;
- avoid overlapping visible ranges unless the existing "newer/larger range
  hides older contained range" policy is intentionally extended to Hazels.

If activation needs range parsing shared by Fiver and Hazel, prefer a small
local helper over a broad lifecycle abstraction.

## Out Of Scope

Do not include these in the immediate no-merge Hazel visibility step:

- background Fiver-to-Hazel conversion;
- Hazel-to-Hazel merge scheduling;
- mixed Fiver/Hazel physical posting merge;
- async Bigwig cache fill;
- Fiver retention policy after Hazel creation;
- preference policy when both a Fiver and exact Hazel exist;
- a new Fluffle shard superclass unless the no-merge implementation becomes
  awkward without it.

## Verification

Follow the repository rule: compile/build checks only unless the user
explicitly asks for runtime experiments, ranking runs, evals, or benchmarks.

Likely compile checks:

```sh
bazel build //src:cottontail
bazel build //test:tests
bazel build //test:hazel_test
bazel build //apps:trec-example
```

Do not run `bazel test`, ranking, evals, benchmarks, or `apps/trec-example`
unless explicitly requested.

## Useful Starting Points

- `src/bigwig.cc`
- `src/bigwig.h`
- `src/fluffle.h`
- `src/fiver.cc`
- `src/fiver.h`
- `src/hazel.cc`
- `src/hazel.h`
- `src/vector_hopper.cc`
- `src/owsla.h`
- `apps/fiver2hazel.cc`
- `test/hazel.cc`
