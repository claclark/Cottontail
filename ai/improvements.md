# Potential Improvements

Concise list of possible cleanups, design questions, and follow-up work. These
are not committed plans unless the user promotes them into an active plan or
explicit coding task.

Especially don't do these things without discussion and approval from the user.

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
