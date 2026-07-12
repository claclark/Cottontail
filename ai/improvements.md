# Potential Improvements

Concise list of possible cleanups, design questions, and follow-up work. These
are not committed plans unless the user promotes them into an active plan or
explicit coding task.

Especially don't do these things without discussion and approval from the user.

## Cached Phrase Postings

- Formalize the existing rule that token text does not contain ASCII whitespace
  or control characters. A normalized token sequence can then be serialized
  with a reserved separator and featurized as a phrase cache key without
  colliding with an ordinary token feature.
- Add a phrase operation to `Warren` that returns a hopper. Its default
  implementation should construct exactly the phrase hopper used today, so
  Warrens without phrase caching preserve current behavior.
- Phrase lowering should call the Warren operation rather than assemble the
  phrase entirely inside GCL. A cache-aware Warren can get or reserve a
  waitable posting-cache entry under the canonical phrase feature.
- When a phrase first reserves its cache entry, launch a worker to obtain the
  component token postings, solve the phrase with current interval semantics,
  fill a `SimplePosting`, and release the cache entry. Query construction can
  continue in parallel; a consumer waits only if it reaches the phrase before
  the posting is ready.
- Concurrent queries should share the same in-progress phrase entry. The cache
  generation must remain tied to the Warren read snapshot so a visible commit
  naturally invalidates derived phrase postings along with other cached
  postings.
- Decide the exact `Warren::phrase(...)` signature and canonical phrase-key
  encoding during design. The operation should receive normalized phrase
  components rather than depend on the original surface spelling.

## Parallel Multi-Burrow Activation

- Load `ssr-server` collections concurrently, with one activation task per
  requested burrow. Join all activation tasks before serving requests, preserve
  command-line collection order, and report activation or GCL-validation errors
  after the join.
- Implement parallel activation together with the planned "burrow is already
  open" mechanism. Opening should use canonical burrow identity and an atomic
  get-or-open operation: one caller performs activation, concurrent callers wait
  for it, and later callers reuse the existing open dynamic state.
- Do not reject repeated burrow arguments. They should reuse the existing
  Fluffle rather than create independent Fluffles that can concurrently merge
  the same directory. Callers may receive separate Warren views where required.
- Give each Bigwig the total number of distinct Bigwigs active in the process.
  This is workload context, not a caller-selected merge-thread count. Repeated
  arguments and additional Warren views do not increase the total.
- Bigwig should combine that population count with the machine's normal worker
  budget to derive its own merge-worker allowance. Threading and merge-policy
  decisions remain owned by Bigwig, while multiple open corpora no longer each
  assume they own the entire machine.
- Serial opening already leaves earlier Fluffles merging while later burrows
  activate, so parallel activation does not create a fundamentally new merge
  load. Its purpose is to reduce startup latency while making existing
  multi-Bigwig resource use explicit.

## Restartable builder for static shards

- After the Meadowlark `append_*` lifecycle is finished, add a restartable tool
  that greedily balances whole input files across standalone Hazel shards. The
  durable manifest, serial ingestion, live Bigwig consolidation, restart, and
  final publication design is in `ai/static-shards.md`.

## Directory-level locking

- Ensure only one process (i.e., one flufle) is manipulating the databases at
  any one time.
- Implement via a lock file with a clearly explained clean-up process.
- Probably call the lock file "LOCKED.sh" and you should be able to unlock by
  running it. With internal documentation explaining it.
- Clean-up should include deleting partial Hazel merges, since we are assuming
  a hard crash, rather than a clean shutdown.

## Working File Operations

- Audit file operations on working-directory contents and route them through
  `Working` where possible.
- `Working` should remain the place that owns path-name construction,
  temporary-name conventions, working-directory cleanup, and checked removal.
- Keep arbitrary full-path or non-working-directory operations separate, so the
  boundary stays clear.

## Concurrent Shard Activation

- Bigwig startup currently activates sanitized shards serially.
- Consider activating independent Hazel/Fiver shards concurrently after
  `sanitize(...)` has established the visible shard order and recovery
  invariants.
- Preserve deterministic Fluffle ordering: concurrent workers may open/start
  shards in parallel, but publication into `fluffle->warrens` should follow the
  sanitized `[ Hazel prefix ][ Fiver suffix ]` inventory order.

## SimpleIdx Cache Policy

- Revisit the SimpleIdx posting-cache strategy before making the current
  disabled eviction path permanent.
- The old threshold-based large-posting eviction policy can discard expensive
  decoded postings and depends on size assumptions that do not scale well from
  smaller indexes to TB-scale shards.
- If bounded cache behavior is needed, consider replacing the old thresholds
  with an LRU-style policy or another policy tied more directly to observed
  memory pressure and reuse.

## Ranking Content and Container Terminology

- Untangle places in `ranking.cc` and adjacent app code where "content" means
  the rankable interval and where "container" means the outer document/item
  interval.
- SSR server usage now needs all three concepts: an outer container that holds
  identity and displayable document text, a content interval used for ranking,
  and a docno/id interval used for lookup.
- BM25-style annotations are associated with the container, so future API and
  variable naming should make that boundary explicit rather than assuming one
  interval can serve every ranking model.

## General Parallel Ranking Server

- Extend `ssr-server` beyond SSR to support BM25 and other ranking methods over
  the same multi-collection request and result-merging framework. The server and
  clients should probably be renamed once SSR is one ranking mode rather than
  the identity of the service.
- Give requests an explicit desired global result depth `m`. For independently
  partitioned shards, choose a smaller local retrieval depth `k` from the shard
  count `n` and an acceptable miss probability instead of always retrieving the
  full global depth from every shard.
- Use the Section 4 model from Clarke and Terra, [Approximating the Top-m
  Passages in a Parallel Question Answering
  System](https://plg.uwaterloo.ca/~claclark/top.pdf). Its dynamic-programming
  recurrence computes the probability that taking the local top `k` from each
  of `n` randomly populated shards contains the global top `m`.
- Choose the smallest `k` meeting a configured confidence threshold, then merge
  local results normally. Preserve a conservative exact mode with `k = m`.
- State and check the model assumptions: shards should be roughly balanced and
  target items approximately uniformly and independently distributed. Repeated
  or content-partitioned collections do not satisfy the model and need a
  conservative fallback or a different model.
- This optimization is not especially important for the current interactive
  SSR use, but becomes useful when the server exposes general top-`m` ranking
  with BM25 and other methods.

## Txt Wrapping

- Preserve `Txt::translate(...)` as an index-facing representation that the
  configured tokenizer can split back into the same token stream. Internal JSON
  structural tokens belong in that representation; presentation conversion
  belongs at application display boundaries through `json_translate(...)`.
- Audit and eventually remove or simplify `JsonTxt` presentation wrapping. It
  currently makes storage-level translation depend on display concerns and
  obscures the distinction between faithful translation and human-readable
  rendering.
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
- Preserve lower-level `ready_()` / publication errors instead of collapsing
  them to only `"Transaction cannot be commited."`, especially around dynamic
  Bigwig/Fiver transaction readiness.
- Hazel cache loading should eventually use this for corrupted posting reads or
  decode failures before returning structured bogus fallback data.
- Replace failsafe NullAppender/NullAnnotator with error logging equivalents.

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

## Hazel Merge Unique-Source Fast Path

- Reintroduce a narrow fast path inside restartable `HazelIdx::merge` for
  ordinary features that occur in exactly one input Hazel when there is no
  active `null_feature` exclusion posting.
- For non-inline postings, copy the source compressed posting bytes into the
  checkpoint `.pst` file and then append the corresponding `.dct` entry as the
  commit marker.
- Preserve inline singleton postings as dictionary-only checkpoint entries
  without decoding/re-encoding.
- Do not apply this path to `null_feature` or the text-chunk posting. The
  text-chunk posting still needs text-base value adjustment during merge.

## Hazel Merge Disk Usage

- Restructure Hazel posting-list merge so merged postings can be written
  directly into the restartable `mrg.*` files instead of first materializing a
  separate full posting output and then checkpointing or publishing it.
- Large Hazel/Hazel merges can currently require roughly twice the final disk
  footprint during the merge. Direct-to-`mrg.*` output should reduce peak disk
  pressure, which matters for multi-terabyte builds.
- This may require separating posting merge production from current file
  publication assumptions so dictionary/checkpoint entries remain the durable
  commit markers while posting bytes stream directly to their final recoverable
  merge location.

## Split Test Targets and improve regression testing generally

- The old aggregate `//test:tests` target makes it awkward to run or reason
  about one focused regression at a time.
- Use `//test:hazel_test` as the pattern: move coherent test files or families
  into dedicated `cc_test` targets with clear names, while keeping an aggregate
  target for broad checks if it remains useful.
- Smaller targets should make individual failures faster to isolate, reduce
  accidental coupling between unrelated tests, and let slow or specialized
  regressions declare their own size/resources.
- When splitting, keep shared fixtures explicit and avoid hiding runtime-heavy
  tests inside broad default targets.
- Review regression test suite for deficiencies.
