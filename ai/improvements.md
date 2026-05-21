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
