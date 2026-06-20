# Bigwig/Hazel Integration Checkpoint

This file records the current Hazel integration state after the 2026-06-20
merge-worker policy work. It is a checkpoint, not blanket authorization for the
next coding step. The user will continue to choose the next change.

## Current State

- `Owsla` is the common shard interface for posting-capable Bigwig children.
  Fiver and Hazel both expose `posting(feature)`, cheap cached
  `estimated_size()`, `get_sequence(...)`, and `discard(...)`.
- Bigwig startup runs `sanitize(...)`, activates the sanitized Hazel prefix
  followed by the Fiver suffix, and stores the visible set as
  `std::vector<std::shared_ptr<Owsla>>`.
- Fluffle owns the shared working context, Bigwig cache generation, current
  visible shard vector, merge-in-progress set, and sanitized pending Hazel
  merge recovery records.
- Bigwig read snapshots compose visible `Owsla` children. Normal multi-shard
  posting merges are cached through `OwslaCache`; `text_chunk_tag` remains
  mergeable but uncached.
- The default working-directory `Fiver::hazel(...)` overload writes and
  activates an unstarted Hazel. The explicit-filename overload remains a
  bool-returning writer.
- The activated-source `Hazel::merge(hazels, dst, ...)` overload writes and
  activates an unstarted output Hazel. The working/name adapter remains
  bool-returning for existing command-line callers.
- Source shard deletion is now lifecycle policy in `merge_worker`, not part of
  Fiver-to-Hazel conversion. After successful Fluffle publication, selected
  source shards are discarded through `Owsla::discard(...)`.

## Merge Worker Shape

`merge_worker(...)` is now organized around one policy function:

```cpp
find_merge_action(fluffle, &start, &end)
```

The policy recommends a visible shard range by index. The worker, while holding
the Fluffle lock, validates the recommendation, classifies it as one of:

- Fiver/Fiver merge,
- Hazel/Hazel merge,
- boundary Fiver-to-Hazel conversion,

claims the selected shards in `fluffle->merging`, optionally spawns a friend
worker, releases the lock, performs the operation, reacquires the lock, and
publishes the replacement shard.

Worker exits that observe no usable work or a terminal publication failure
decrement `fluffle->workers` while still holding the Fluffle lock. This avoids
the lost-worker race where `try_merge()` could see a retiring worker counted,
decline to spawn a replacement, and then have that worker exit.

## Current Policy

Hazel-aware policy is currently enabled.

Policy order inside `find_merge_action(...)`:

1. Compute `hazel_merge_okay(...)`.
2. If Hazel work is allowed, try boundary Fiver-to-Hazel conversion first.
3. Merge a run of small available Fivers, preserving the old Fiver behavior.
4. Merge the smallest available adjacent Fiver/Fiver pair.
5. If Hazel work is allowed, merge the smallest available adjacent Hazel/Hazel
   pair.
6. Otherwise report no recommendation.

The boundary Fiver conversion rule converts the first Fiver after the Hazel
prefix if it is at least 128 MiB, or if there is at least one Hazel and it is
the only live Fiver.

The current Hazel work gate is intentionally simple: count Hazel shards already
in `fluffle->merging`; allow another Hazel-related action only when
`merging_hazels + 1 < fluffle->max_workers`.

## Testing Status

The user reported successful regression/ranking checks for:

- single live Hazel activated directly and through Bigwig;
- multiple live Hazels;
- mixed `[ Hazel prefix ][ Fiver suffix ]` directories;
- boundary-shadow cleanup where a Hazel covers a redundant suffix Fiver;
- background Fiver suffix consolidation while a Hazel prefix remains;
- `build.sh` and `rank.sh` after Hazel policy work, including repeated stress
  runs that did not reproduce the one intermittent create failure.

One intermittent `meadowlark --create --tsv` ready failure was observed during
policy testing and then did not reproduce across many repeated create/create
plus forage runs. The practical follow-up if it reappears is to stop before
running any second command and capture the working-directory file list,
especially `kitten.*`, `temp.*`, `mrg.hazel.*`, `pst.hazel.*`, `dct.hazel.*`,
and overlapping shard ranges.

Repository rule remains: agents should run compile/build checks only unless the
user explicitly asks for runtime experiments, ranking runs, evals, or
benchmarks.

## Likely Follow-Ups

- Add or refine focused tests around `find_merge_action(...)` and the worker
  classification/publication paths.
- Revisit the Hazel work gate if the current shard-count approximation does
  not give the desired worker mix under heavy concurrency.
- Decide whether recovered `HazelMergeRecovery` records should be scheduled
  ahead of new Hazel/Hazel merges.
- Improve lower-level error propagation from `ready_()` paths so failures do
  not collapse into only `"Transaction cannot be commited."`
- Consider concurrent shard activation after `sanitize(...)` has established a
  deterministic inventory order.
