# Current Plan

## Working Rule

No coding without discussion and explicit confirmation from the user.

Work on one agreed step at a time. Approval for one step does not
authorize starting a later step, and entries in `ai/improvements.md`
remain speculative until the user promotes them into the active plan.

Do not run ahead into adjacent refactors while implementing an agreed
step. If the work appears to require a broader abstraction or a materially
different design, stop and discuss it first. Proposals for refactoring,
code improvement, (near-)clone reduction, or anything that improves
code quality are welcome, but discuss with the user. Never hesitate to
discuss a potential quality improvement.

## 1. Meadowlark Append Lifecycles (Current Formats Complete)

The JSONL and TSV file appenders now share a consistent, restartable lifecycle.
The remaining append surfaces are future steps rather than active work.

- Preserve the coordinated streaming, start/end, source identity, metadata,
  commit, abort, and restart behavior of the JSONL and TSV file paths,
  including canonical `/` identities and transaction-local `//` names inside
  `/.` source segments.
- Keep Meadowlark argument parsing and typed input records reusable so future
  callers do not duplicate the CLI's interpretation of input files.
- Finish the planned append surfaces only after discussing each one: JSONL from
  strings with explicit source identity, raw text files, and code as raw text
  plus metadata.
- Preserve the central restart invariant: rerunning Meadowlark with the complete
  input list skips committed sources and safely retries missing or interrupted
  sources.

The format and append conventions are recorded in `ai/meadowlark.md`. JSONL and
TSV satisfy the ingestion prerequisite for an initial restartable static-shard
builder. Any future input adapter must meet the same lifecycle before that
builder accepts it.

## 2. Cache Phrase Postings

After the append work, and only after a separate design confirmation, make
phrases capable of behaving like cached terms.

- Add a phrase operation to `Warren` that returns a hopper; the default
  implementation preserves the phrase behavior used today.
- Use normalized phrase components and the no-ASCII-whitespace/control token
  invariant to construct a canonical derived feature.
- Let cache-aware Warrens reserve a waitable phrase posting, launch phrase
  solving in a worker, fill a `SimplePosting`, and share the result across
  concurrent queries and the current read snapshot.

The current sketch is at the top of `ai/improvements.md`. Related materialize
history and speculative optimizer directions are in `ai/gcl-optimizer.md`.

## 3. Build Restartable Static Shards

After Meadowlark append replay is dependable, implement the static-shard
builder described in `ai/static-shards.md`.

The initial invocation should only publish an immutable, human-readable plan
and then enter the same restart path used after interruption. Restart replays
that plan from the beginning: completed Meadowlark inputs skip, Bigwig merging
resumes, and published standalone Hazels return immediately. The builder uses
greedy whole-file balancing, serial Meadowlark ingestion, concurrent background
consolidation, in-burrow Fiver conversion, and restart-safe Hazel publication.

The JSONL/TSV Meadowlark prerequisite is complete. Do not begin this
implementation unless the user explicitly selects it and approves coding it.
