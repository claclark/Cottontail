# Plan: Ranking Metadata And Parameters

## Context

Hazel activation now works far enough to open a standalone Hazel shard and run
queries against it. `fiver2hazel` preserves the owner Warren `parameters` block,
so a Meadow-derived Hazel is recognized as `format:"meadowlark"`.

The next blocker is ranking setup, not basic Hazel idx/txt activation. Running
the threaded BM25 script against a standalone Hazel reaches
`meadowlark::TfIdfStats::make(...)`, then fails when that code calls:

```
warren->set_parameter("stemmer", stemmer_name, error)
```

`Hazel` is immutable and currently rejects all `set_parameter(...)` calls, so
the ranker exits with `Hazel can't set its parameters`.

## Design Issue

This is not just a Hazel bug. It exposes an older global-parameter model:

- Warren-level parameters currently mix persistent collection metadata with
  runtime/session ranking configuration.
- Meadowlark ranking metadata already stores more precise per-ranking-view
  details in tf-idf forager JSON (`gcl`, `container`, `id`, `stemmer`,
  `tokenizer`).
- The long-term direction is for a database / Meadow to hold many different
  collections, views, stemmers, tokenizers, ids, containers, and ranking
  metadata objects, rather than one global Warren parameter set.

## Immediate Goal

Make Meadow/Hazel ranking work without strengthening the old global-parameter
model more than necessary.

## Suggested Next Step

Inspect `meadowlark/tf-idf_stats.cc` and decide whether
`TfIdfStats::make(...)` actually needs to mutate the Warren.

Likely direction:

- Treat the tf-idf metadata object as authoritative for ranking setup.
- Construct `stats->stemmer_` from the metadata directly.
- Avoid requiring `warren->set_parameter("stemmer", ...)` to succeed.
- Keep any Warren-level `stemmer` parameter as optional compatibility metadata,
  not a required side effect.

## Compatibility Options

Option A: adjust `TfIdfStats::make(...)`

- Best aligned with the future metadata direction.
- Remove or soften the hard failure around `set_parameter("stemmer", ...)`.
- Ranking clones should still work because `TfIdfStats::make(...)` rereads the
  tf-idf metadata for each cloned Warren.

Option B: let `Hazel::set_parameter_("stemmer", ...)` succeed in memory only

- Smaller local compatibility shim.
- Keeps static Hazel files immutable.
- But it continues to bless the old Warren-global mutation path.

Prefer Option A unless a quick experiment shows clone/ranker behavior depends
on `warren->stemmer()` being updated.

## Validation

After the change:

- Rebuild `//apps:working`.
- Rebuild a fresh Hazel from `a.meadow/fiver...` with `//apps:fiver2hazel`.
- Run the user's `rank.sh` against the standalone Hazel shard.
- Compare behavior against `rank.sh a.meadow`; exact runtime may differ, but
  the Hazel run should get past stats setup and produce ranked output.

