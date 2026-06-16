# Goal: Add Hazel Shards To Bigwig Read Snapshots Without Merging

Hazel, Fiver, and Bigwig have been aligned enough to take the next small
integration step:

- `SimplePosting` is storage only. It no longer manufactures hoppers.
- `ArrayHopper` binds directly to waitable `SimplePosting` storage.
- `Owsla` exists as the narrow Warren subclass for posting-capable shards.
- Fiver subclasses `Owsla`; Hazel exposes the same concrete
  `posting(feature)` operation and should join `Owsla` when it becomes
  Bigwig-query-visible.
- Hazel idx cache entries are waitable `SimplePosting`s, not `CacheRecord`s`,
  and use `OwslaCache`.
- Fluffle owns the Bigwig merged-posting cache generation as an `OwslaCache`.
- A Bigwig start captures both the visible shard snapshot and the current cache
  generation.
- The Fluffle cache generation is replaced when a kitten is committed into a
  visible Fiver, not when the write is merely prepared.
- `ArrayHopper` has explicit singleton and singleton-interval coverage; Fiver
  also restores its in-memory singleton fast path.
- BigwigIdx owns Fiver-only posting composition and caching today. True
  multi-Fiver cache misses install a closed waitable posting, fill it
  asynchronously, and return a hopper over that posting.
- Regression builds and user-run ranking checks passed after the Owsla cache
  work; measurements are recorded in `ai/hazel-progress.md`.

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
- expand the shard lifecycle machinery beyond what query visibility needs.

Think of this as "Hazel is query-visible" rather than "Hazel participates in
the lifecycle machinery."

This is still a cleanup/alignment step, not new user-visible behavior. The
main alignment target is that Fiver and Hazel expose the same raw posting
operation to Bigwig's read snapshot. Bigwig remains the aggregator over that
snapshot rather than becoming an `Owsla` itself.

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

## Owsla Posting Interface

Use the small common shard interface `Owsla` for the shared operation already
implemented concretely by Fiver and Hazel:

```
std::shared_ptr<SimplePosting> posting(addr feature)
```

Keep this interface narrow. It should not become a lifecycle superclass for
commit, activation, merge policy, text access, or transaction behavior. Its job
is to say: "given a feature, produce the raw posting on the caller's thread, or
return `nullptr` if the shard has none."

Fiver already implements this operation as an `Owsla`. Hazel has the matching
concrete operation and should inherit `Owsla` as part of the mixed read-view
step:

- Fiver returns the in-memory `SimplePosting`;
- Hazel fills or waits on its cached `SimplePosting` synchronously.

Bigwig should not implement `Owsla`. Its idx layer composes postings from the
started child shard snapshot.

`posting(feature)` is storage composition, not query semantics. It does not
apply `null_feature` exclusions.

## Bigwig Read View

Current Bigwig read state narrows to:

```
std::vector<std::shared_ptr<Fiver>>
```

The no-merge Hazel step should change the started view to hold posting-capable
readable shards. The likely shape is:

```
std::vector<std::shared_ptr<Owsla>>
```

The objects in that vector are still Warrens in practice: Fiver and Hazel.
Bigwig text composition still needs the public Warren `txt()` interface, so the
implementation may keep a parallel/read-compatible Warren view or rely on the
fact that `Owsla` is a Warren. Avoid turning Owsla into a broader abstraction
just to serve text.

## BigwigTxt

`BigwigTxt` is the easiest part. It currently stores Fivers, but its operations
already use the public Warren txt interface:

- `warren->txt()->translate(p, q)`
- `warren->txt()->tokens()`
- `warren->txt()->range(...)`

The no-merge Hazel step should make `BigwigTxt` store generic Warrens and keep
that behavior. Do not add text merge logic.

## BigwigIdx

`BigwigIdx` currently owns Fiver-only visible-read posting composition and
cache fills.

For the no-merge Hazel step, `BigwigIdx` should extend that structure to
`Owsla` children:

- raw posting composition does the synchronous work on the caller's thread;
- `hopper(feature)` creates/uses a waitable posting and can start a worker
  thread for cache misses.

BigwigIdx's raw posting composition should:

1. Scan the started shard view with fast `idx()->count(feature)`.
2. Return `nullptr` when no shard contributes.
3. If one shard contributes, delegate to that shard's `posting(feature)`.
4. If multiple shards contribute, collect the child postings and merge them
   into one `SimplePosting`.

This makes Hazel query-visible through the same mergepoint that already works
for Fiver. Nested Bigwigs should not be part of this step.

The Bigwig feature cache belongs at this aggregation layer. It caches merged
raw postings for the captured visible shard snapshot. Cache only true
multi-shard posting merges, and use Bigwig's own compressors explicitly rather
than depending on the first contributing shard for compressor choices.

`text_chunk_tag` is mergeable for query purposes: a merged posting and a
`VectorHopper` over shard text-chunk hoppers are semantically equivalent here.
However, do not store `text_chunk_tag` in the generic Bigwig feature cache.
That cache is for normal feature postings keyed to a sequence snapshot; text
chunk postings are part of the text-composition path and should be produced
fresh or delegated through hoppers.

`hopper(feature)` should wrap the raw posting result in an `ArrayHopper`, or
return `EmptyHopper` when the raw posting is `nullptr`. For asynchronous cache
fills, the hopper can be returned over a waitable `SimplePosting`, as Hazel
does today.

## Deletion Semantics

Deletion behavior should remain compositional:

```
NotContainedIn(feature_hopper, null_feature_hopper)
```

For the mixed no-merge step, both `feature_hopper` and `null_feature_hopper`
can be constructed with the same Bigwig posting/hopper composition rules:

- empty if no shard contributes;
- direct delegation if one shard contributes at the posting layer;
- merged raw posting when multiple shards contribute.

Do not apply exclusions in `posting(feature)`. `null_feature` remains raw data
there, just like any other feature. The topmost query hopper is responsible for
`NotContainedIn(feature_hopper, null_feature_hopper)`.

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
- mixed Fiver/Hazel physical shard merge;
- background cache prewarming or fill work not tied to a requested hopper;
- Fiver retention policy after Hazel creation;
- preference policy when both a Fiver and exact Hazel exist;
- broad lifecycle abstraction beyond the narrow posting-capable Owsla
  interface.

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
