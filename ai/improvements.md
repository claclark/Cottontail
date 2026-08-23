# Potential Improvements

Concise list of possible cleanups, design questions, and follow-up work. These
are not committed plans unless the user promotes them into an active plan or
explicit coding task.

Especially don't do these things without discussion and approval from the user.

## Warren Memory Trimming

- Long-running services over large ClimbMix indexes need a way to release
  reconstructible Warren memory before unbounded cache growth destabilizes the
  host.
- Keep process measurement, high-water admission control, polling, restart,
  and escalation policy outside Cottontail. The service can call the public
  `Warren::trim_memory()` on every active Warren and clone.
- Trimming should preserve semantics and durable state while attempting a
  substantial reduction, nominally around half of reconstructible retained
  memory. This is an aspiration rather than a total-memory guarantee.
- The narrow first implementation clears Hazel decoded postings through an
  internally locked, in-place `OwslaCache::clear()`. Active hoppers retain
  shared posting ownership, later queries refill, and duplicate calls through
  clones are harmless. Other Warren implementations currently do nothing.
- A future Bigwig implementation must trim both current Fluffle state and the
  historical snapshot held by the particular started view.
- Hazel decompressed text chunks are not trimmed: translations temporarily
  borrow raw pointers from `unique_ptr` cache entries, so concurrent eviction
  needs a clean lifetime or locking design.
- The complete discussion and deferred questions are in `ai/memory.md`.

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

## Coordinated Commit Visibility

- `Warren::commit_all(...)` publishes already-readied Fivers sequentially, so a
  new read epoch can currently capture a proper subset during the short commit
  window even though recovery will normally finish the complete set.
- Add a Fluffle-level publication gate for coordinated commits. Set it before
  the first Fiver commit, make new `Bigwig::start()` operations wait, and clear
  it only after the last commit. Existing read epochs retain their old snapshot;
  newly admitted epochs then see the complete new set.
- The gate need not hold the Fluffle mutex across commit calls that acquire that
  mutex themselves. A flag plus waiting mechanism avoids recursive locking and
  preserves parallel, expensive `ready()` work while serializing only the fast
  publication phase. Cross-process exclusion remains the separate
  directory-locking question below.

## Restartable Builder for Static Shards

- JSONL, TSV, text, and code now have the coordinated, restartable Meadowlark
  append lifecycle needed by an initial builder. Add a separately approved tool
  that greedily balances those supported input files across standalone Hazel
  shards. Any future input type must acquire the same lifecycle before the
  builder accepts it. The durable manifest, ingestion, live Bigwig
  consolidation, restart, and final publication design is in
  `ai/static-shards.md`.

## Meadowlark Metadata Evolution

The established discovery and record conventions live in `ai/meadowlark.md`:
agents bootstrap with `/` and `@`, new metadata is explicitly typed JSON rooted
at `@`, current file-specific records connect to their source through
`filename` (with historical `file` accepted by readers), and `//` inside `/.`
supports inverse lookup from an interval to its source filename. Potential
follow-ups are:

- Define an explicit replacement/recomputation lifecycle before allowing a
  current forager `(name, tag)` to change. The implemented rule is one immutable
  query and parameter map per pair; exact reuse is accepted and a conflicting
  definition is rejected.
- Decide whether source identities need stronger canonicalization than the
  current rule of prefixing `./` only when a filename contains no slash.
- Keep the TSV `columns` record defined by the first row. If callers eventually
  need metadata for lazily tolerated extra columns, design that without adding
  a preliminary full-file scan or weakening coordinated publication.
- Retain missing-`type` and `@tf-idf:` handling as explicit legacy forager
  compatibility until there is a deliberate database migration policy.
- Require each future file-source adapter to define its `type` record, emit a
  transaction-local `//`, and coordinate that metadata with its canonical `/`
  marker and data under the established commit/recovery protocol. Nonempty data
  belongs with `//` inside `/.`; tokenless input deliberately has no `/.`, `:`,
  or filename-feature interval.
- Add a library-level discovery convenience only if repeated consumers need
  one. The `@` record and `:type:` field convention already permits unknown
  types to be inspected or ignored without global component registration.

## Directory-level locking

- Ensure only one process (that is, one Fluffle) is manipulating a database at
  any one time.
- Implement via a lock file with a clearly explained clean-up process.
- Probably call the lock file "LOCKED.sh" and you should be able to unlock by
  running it. With internal documentation explaining it.
- Unlock cleanup should run the normal shard sanitizers. Valid restartable
  merge state should remain recoverable; incomplete, obsolete, or conflicting
  merge files should be discarded by the same rules used at ordinary startup.

## Working File Operations

- Audit file operations on working-directory contents and route them through
  `Working` where possible.
- `Working` should remain the place that owns path-name construction,
  temporary-name conventions, working-directory cleanup, and checked removal.
- Keep arbitrary full-path or non-working-directory operations separate, so the
  boundary stays clear.
- `Working` still shells out to `/bin/mkdir -p` when creating a working
  directory, `/bin/rm -f` when removing stale `temp.*` files, and `/bin/ls` or
  `/usr/bin/ls` when listing contents. If a concrete portability, correctness,
  or error-reporting problem justifies changing this, replace those operations
  with C++17 `std::filesystem`; do not undertake the cleanup merely because the
  standard-library operation now exists.
- Existing regression coverage would catch gross failures: ordinary
  `Working::mkdir(...)` is used throughout the suite, and Bigwig/Hazel tests
  depend heavily on `Working::ls(...)`. Before changing the implementation,
  add focused coverage for nested and punctuation-bearing paths, `temp.*`
  cleanup that preserves unrelated files, prefix listing, and relevant error
  paths.
- The separate application `walk_filesystem(...)` helper skips symlinked files
  and directories, including a symlink supplied as its top-level path. It has
  no focused runtime regression coverage; such coverage should include a
  single file, recursive directories, README inclusion, symlink skipping, an
  empty directory, and a nonexistent path.

## Concurrent Shard Activation

- Bigwig startup currently activates sanitized shards serially.
- Consider activating independent Hazel/Fiver shards concurrently after
  `sanitize(...)` has established the visible shard order and recovery
  invariants.
- Preserve deterministic Fluffle ordering: concurrent workers may open/start
  shards in parallel, but publication into `fluffle->warrens` should follow the
  sanitized sequence-range inventory order.

## SimpleIdx Cache Policy

- Keep this separate from the initial Warren/Owsla memory-trimming direction in
  `ai/memory.md`; Simple is explicitly outside that first scope.
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

- Audit and eventually remove or simplify `JsonTxt` presentation wrapping. It
  still applies the lossy `json_translate(...)` display operation at the text
  abstraction boundary. Full stored JSON conversion now has the separate,
  validating `json_convert(...)` operation.
- Revisit `Txt::wrap(...)` and the general wrapper model.
- Clarify whether one recipe should carry both a concrete `Txt` component's
  physical parameters and wrapper-layer parameters.
- Hazel currently follows the existing `Txt::make(...)` pattern: construct the
  concrete `Txt`, then call `Txt::wrap(recipe, txt, error)`. This may be right,
  but static single-file shards make the distinction between physical component
  recipe and wrapper recipe more visible.

## Tokenless Fiver Appends

- `FiverAppender` retains appended bytes even when tokenization produces no
  tokens, but emits no transaction or text-chunk annotations. `Fiver::ready()`
  now pickles that state, so the bytes survive a normal disk-backed
  `transaction()` / `ready()` / `commit()` cycle and subsequent Fiver merges.
- Genuinely write-free Fivers are serialized by the same rule. This provides a
  commit artifact for coordinated transactions without giving the Fiver a
  token range.
- Decide what address and translation semantics tokenless appended text should
  have. Non-token bytes normally live with a preceding token, but a wholly
  tokenless new Fiver has no preceding address to own them.
- Current focused coverage activates empty, tokenless, and tokenful Fivers;
  combines them through flat and tree-shaped merges; and converts empty,
  tokenless, and mixed results to Hazel. This establishes durability and merge
  behavior without inventing an address for leading dust.
- Fiver-to-Hazel conversion stores leading tokenless bytes from raw byte zero
  while retaining the first token chunk's original later byte anchor. This
  keeps the Hazel text directory honest without assigning the prefix a token
  address.

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

## Hazel Posting-Log Follow-Ups

The restartable posting-log Hazel merge is complete. These are possible
follow-ups, not standing authorization:

- After measuring the posting-log merge, consider copying a non-inline source
  record directly into a worker segment when an ordinary feature occurs in
  exactly one Hazel and no exclusion posting can change it.
- Measure peak disk use while restart logs, source Hazels, and final assembly
  coexist. The initial design favors simple recovery and compressed-record
  assembly; a later design may need to reduce that overlap for very large
  static shards.

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
- The 2026-08-23 audit added focused file-oriented forager and empty/tokenless
  Fiver cases, and the user reports that the complete suite plus additional
  tests pass. Continue reviewing for semantic gaps as code changes, but the
  immediate issue here is target size and isolation rather than a known failing
  subsystem.
