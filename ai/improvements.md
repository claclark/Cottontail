# Potential Improvements

Concise list of possible cleanups, design questions, and follow-up work. These
are not committed plans unless promoted into `ai/plan.md`.

## Txt Wrapping

- Revisit `Txt::wrap(...)` and the general wrapper model.
- Clarify whether one recipe should carry both a concrete `Txt` component's
  physical parameters and wrapper-layer parameters.
- Hazel currently follows the existing `Txt::make(...)` pattern: construct the
  concrete `Txt`, then call `Txt::wrap(recipe, txt, error)`. This may be right,
  but static single-file shards make the distinction between physical component
  recipe and wrapper recipe more visible.

## Deep Error Logging

- Add a logging path for deep internal errors that currently only assert.
- Consider making `safe_error(...)` also log to stderr when it records an
  error, then add a separate helper for invariant/deep-format failures that
  should be visible even when assertions are disabled.
- Hazel cache loading should eventually use this for corrupted posting reads or
  decode failures before returning structured bogus fallback data.

## Text Deletions

- Current Fiver/Hazel/Bigwig merge behavior may preserve physical text bytes
  even when ordinary postings covered by `null_feature` are removed from the
  merged idx.
- Translation semantics should remain logical rather than physical: if a
  requested range touches excluded/deleted text, return an empty string instead
  of splicing surrounding text together.
- Long-term compaction can reclaim storage by omitting txt chunks that are
  fully covered by exclusions, while preserving the invariant that tokenizing
  translated text yields the indexed tokens for the requested range.

## Exclusion Merge Semantics

- `null_feature` / exclusion postings may need an outer interval merge rather
  than the ordinary `SimplePostingFactory::posting_from_merge(...)` semantics.
- Fiver/Hazel/Bigwig merge paths should preserve the full covering exclusion
  intervals so ordinary postings are removed anywhere the logical deletion
  applies.
- Consider adding a dedicated exclusion/null merge helper instead of relying on
  ordinary posting-list merge behavior.
