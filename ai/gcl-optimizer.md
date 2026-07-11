# GCL Optimizer Checkpoint

This note records the current GCL optimizer after the 2026-07-04 SSR query
performance work.

## Current Position

- GCL lives in the top-level `gcl/` directory and remains part of the core
  Cottontail library.
- Query optimization is an S-expression rewrite after phrase expansion and
  before hopper construction.
- Optimization is enabled by default. `Optimizer::disable()` remains available
  for explicit comparisons, and `apps/ssr-server` exposes the switch through
  its `set_optimizer` protocol command.
- The previous estimated-count rewrite for top-level
  `(<< (^ a b c ...) Q)` expressions was removed. The active rule is structural
  and does not consult collection statistics.

## Current Rewrite

The optimizer materializes containment used as an operand of combination. A
single tree walk carries materialization context downward from `+` nodes. The
first containment operator reached under that context (`<<`, `>>`, `!<`, or
`!>`) is wrapped in `(materialize X)`. Its subtree is visited without the
inherited context, although another nested `+` can turn the context on again.

A materialized expression is treated as a term for optimization further up the
tree. This allows nested combinations to materialize the useful containment
boundaries without repeatedly materializing every node below them.

Phrase expansion produces a `>>` containment around an ordered-window
expression. Consequently, a phrase under `+` is materialized at that outer
containment rather than at the phrase-internal `...` operator.

## Materialize

On first use, the active `Materialize` hopper fully enumerates its child with
successive `tau(p + 1)` calls and delegates later access to the stored result.
It preserves empty and singleton fast paths. Starting with the second result,
it writes directly into vector-backed `SimplePosting` storage and returns an
`ArrayHopper` over that posting, avoiding the old temporary-vector plus
shared-array copy.

`SimplePosting` omits `q` storage while `q == p` and omits `v` storage while all
values are `0.0`; the hopper restores those semantic defaults.

### Lazy Materialization Experiment

Lazy materialization was implemented and measured, not merely proposed. One
motivating slow query was:

```text
(^ (+ "animal testing" "tested on animals")
   (+ "by" students student)
   (+ "boycott" "cruelty-free" "make this issue known"))
```

Explicitly materializing its phrase alternatives made it faster. A brief first
experiment sampled accessor latency and switched to full materialization after
a slow call. That behavior was removed because timing-dependent execution was
fragile and counter-intuitive.

The retained lazy implementation is deterministic sparse memoization. Each of
`tau`, `rho`, `uat`, and `ohr` keeps its own ordered map of solved answers and
the key interval for which each answer remains valid. A lookup reuses a valid
entry; otherwise it asks the child hopper and stores the answer. If the new
answer has an endpoint already in the map, the entry's valid interval is
widened. Unlike the active implementation, this mode never enumerates the
entire child solely to materialize it.

The sparse implementation was slower than full materialization in the sampled
SSR runs, so full materialization remains the default. The lazy code is retained
in `gcl/materialize.*` behind `COTTONTAIL_GCL_MATERIALIZE_LAZY` and can be
enabled for experiments by defining it to `1`, for example:

```sh
bazel build --cxxopt=-DCOTTONTAIL_GCL_MATERIALIZE_LAZY=1 //apps:ssr-server
```

## Hopper Memoization

The same work broadened `Hopper` accessor memoization from exact-key reuse to
reuse over the interval for which a cached answer remains valid:

- `tau`: cached `p >= k >= cached-k`
- `rho`: cached `q >= k >= cached-k`
- `uat`: cached `q <= k <= cached-k`
- `ohr`: cached `p <= k <= cached-k`

These checks preserve accessor semantics and are expected to be inexpensive on
modern processors because a workload tends either to reuse them frequently or
to miss them consistently.

## Measurement And Correctness

`apps/ssr-timing` runs each tagged query as cold/optimized, warm/unoptimized,
and warm/optimized, flushing each server-reported timing and checking returned
docnos. The sampled runs produced no docno mismatch reports.

The timing corpus was a stratified sample from roughly 5,000 queries whose
original distribution was dominated by sub-second cases but included one query
near 30 minutes. Informal reweighting by the original latency buckets suggested
that the memoization and optimizer work together reduced average query time by
roughly one quarter and removed the extreme tail. Some queries containing very
common phrases, notably `united states`, became slower because full
materialization can be large, although the worst optimized samples were still
on the order of seconds rather than minutes.

The focused optimizer tests cover `+`-driven containment materialization. Hopper
and materialization tests cover widened accessor memoization and
`SimplePosting`-backed materialization semantics. The user ran `make testing`
successfully after these changes. MARCO dev-small ranking checks also preserved
the query count and nearly identical MRR; inspected differences were small rank
or top-10 boundary changes consistent with independently built collection
order.

## Follow-Up Directions

### Phrase Posting Production

A possible future step is to make phrases first-class posting producers in the
same sense as terms. Term postings can already be decompressed and merged in a
worker; a phrase posting worker could additionally solve the phrase before
publishing its result. This is only a direction for later design, not current
optimizer behavior.

### Speculative Substitution Bindings

A larger speculative design is an optimizer-generated GCL operator for staged
materialized bindings, tentatively shaped like `(substitute A B C ...)`. Each
stage would be evaluated in order and materialized. Later stages could refer to
earlier results with `($ 1)`, `($ 2)`, and so on.

These references would be reusable bound values, not one-time hoppers passed
down a single branch. A materialized result could appear multiple times in
later expressions without recomputing the stage. This could express rewrites
that first build a selective `(<< (^ a b) Q)` seed and then substitute it into
later refinements, avoiding eager global work on frequent terms inside generic
boolean operators.

This is substantially larger than the current `materialize` wrapper. It would
need design work around reference lifetime, sharing, S-expression
representation, and how `($ n)` lowers to hoppers before any implementation is
considered.

## Recommendation

Keep the optimizer enabled by default and retain the explicit disable path for
comparison and diagnosis. Continue watching common-phrase materialization costs
as query and collection mixes broaden.
