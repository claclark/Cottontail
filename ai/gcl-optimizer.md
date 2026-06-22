# GCL Optimizer Checkpoint

This note records the current GCL optimizer experiment. It is a checkpoint for
future development, not a commitment to turn the optimizer on by default.

## Current Position

- GCL lives in the top-level `gcl/` directory and remains part of the core
  Cottontail library.
- Query optimization is implemented as an S-expression rewrite step after phrase
  expansion and before hopper construction.
- Optimization is intentionally disabled by default. Callers must explicitly use
  `Optimizer::enable()` for experiments and `Optimizer::disable()` to restore
  the baseline.
- The current rewrite has encouraging timing results once cache reload noise is
  controlled, but confidence is not high enough to make it permanent behavior.

## Pieces

- `gcl/optimizer.*` owns the experimental tree rewrite machinery.
- `gcl/materialize.*` implements the `(materialize X)` operator.
- `gcl/parse.*` recognizes `materialize` as a unary GCL operator and lowers it
  through normal hopper construction.
- `test/optimizer.cc` is the focused optimizer test target.
- `apps/gcl-timing.cc` is a scratch timing app for comparing optimized and
  unoptimized runs. It is useful for local exploration, but should not be
  treated as a stable benchmark or test harness yet.

## Materialize

`(materialize X)` is worth keeping as a GCL feature for manual experiments and
future optimizer-generated rewrites.

The current `Materialize` hopper enumerates `X` once, stores the resulting
`p/q/v` triples into array-backed storage, and delegates future hopper calls to
that materialized result. The operator is parseable GCL, but its main intended
use is still optimizer and performance experimentation.

## Current Rewrite

The current rewrite targets queries shaped like:

```text
(<< (^ a b c ...) Q)
```

where `a`, `b`, `c`, and `Q` are atomic `TERM` or `FIXED` expressions. The
optimizer sorts the all-of atoms by estimated term count, builds a selective
inner contained-in expression, and materializes each nested level. For example:

```text
(<< (^ a b c) Q)
```

becomes conceptually:

```text
(materialize (<< (^ (materialize (<< (^ a b) Q)) c) Q))
```

with `a`, `b`, and `c` ordered by estimated cost.

The rewrite can improve performance, but it also exposes limitations:

- Hopper construction is still eager, so term hoppers can be constructed before
  a selective materialized stage proves the query is empty.
- Generic boolean operators such as `^` can still touch both operands through
  `L`/`R` even when one operand is materialized and selective.
- Timing results are sensitive to posting-cache behavior, especially for very
  frequent terms on large Simple indexes.

## Cache Context

SimpleIdx large-posting cache eviction is currently compiled out with
`COTTONTAIL_SIMPLE_IDX_CACHE_EJECTION` set to `0`. This avoids repeated forced
reloads during timing experiments and better matches the current belief that
throwing away expensive decoded postings is often counterproductive.

There is a separate improvement note to revisit SimpleIdx cache policy and
consider an LRU-style replacement if bounded eviction is needed.

## Substitute Idea

A possible next design is an optimizer-generated staged substitution operator,
tentatively:

```text
(substitute A B C ...)
```

Each stage would be evaluated in order and materialized. Later stages could
refer to earlier materialized results using `($ 1)`, `($ 2)`, and so on.
These references are reusable bound values, not one-time hoppers passed down a
single branch.

The goal is to make a selective materialized subexpression drive later query
stages without recomputing it and without forcing global work on frequent terms
too early. For example, a selective `(<< (^ a b) Q)` result could be bound once
and then referenced repeatedly in later refinements.

This is a larger design than the current `materialize` wrapper. Before
implementation, it needs decisions about reference lifetime, sharing,
S-expression syntax, lowering to hoppers, and interaction with existing GCL
operators.

See `ai/improvements.md`, section `GCL Substitute Bindings`, for the durable
follow-up note.

## Recommendation

Keep the framework and `materialize` support. Keep optimization default-off.
The current results are promising enough to continue exploring, but not strong
or clean enough to lock in as always-on query behavior today.
