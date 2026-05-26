# Goal: Meadowlark Ranking Stemmer Bug

Investigate and fix the Meadowlark ranking path that still depends on a
Warren-global `stemmer` DNA parameter.

Meadowlark ranking-view settings should come from database metadata records
(`@` / `@tf-idf:`), not from the Warren DNA. Older non-Meadowlark formats may
still use DNA parameters for backward compatibility, but Meadowlark should not
require `stemmer:"porter"` in `a.meadow/dna`.

## Observed Behavior

- New `a.meadow` fails ranking unless `stemmer:"porter"` is hacked into DNA.
- Removing that DNA stemmer reproduces the failure.
- Old `b.meadow` has `stemmer:"porter"` in DNA and works.
- The `@` metadata query in `a.meadow` shows tf-idf metadata does contain
  `"stemmer": "porter"`.
- The pipeline under discussion includes an explicit stem stage:

```
bm25:b=0.68 bm25:k1=0.82 bm25:depth=10 stop stem bm25
```

## Starting Points

Review these first:

- `apps/rank.cc`: threaded ranking path creates per-thread `larren` clones and
  calls `Ranker::from_pipeline(pipeline, larren, &error)`.
- `src/ranker.cc`: `StemTransformer` should use `context->stats_->stemmer()`.
- `src/stats.cc`: Meadowlark should route through
  `meadowlark::TfIdfStats::make(...)`; generic Warren stemmer fallback should
  not override Meadowlark metadata semantics.
- `meadowlark/tf-idf_stats.cc`: reads `@tf-idf:` metadata and constructs the
  ranking-view stemmer/tokenizer.
- `src/bigwig.cc`: there is a separate old shadowing bug in DNA stemmer loading;
  fix if relevant, but do not make Meadowlark depend on DNA stemmers.

## Likely Shape

Find the path where `stem` / BM25 is getting a null or Warren-global stemmer
instead of the metadata stemmer. 

Keep the fix scoped. Do not reintroduce writing Meadowlark ranking-view stemmer
settings into DNA.

Do not edit code without instruction from the user. This is an investigation.

## Hazel Note

The Hazel merge implementation has already been written and documented in
`ai/hazel.md`. It is compile-checked only; behavioral tests remain user-guided
pending this bug fix.
