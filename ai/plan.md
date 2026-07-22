# Current Plan

## Working Rule

No coding without discussion and explicit confirmation from the user.

Work on one agreed step at a time. Approval for one step does not authorize
starting a later step. Entries in `ai/improvements.md` are possibilities, not
active work, until the user promotes one into this plan.

If an agreed change appears to require a broader abstraction or materially
different design, stop and discuss it. Proposals for cleanup and refactoring
are welcome, but they also require discussion before implementation.

## Current Checkpoint

The Meadowlark JSONL, TSV, text, and code ingestion work is complete. The
current worktree has passed compile checks, and the user is running the
regression tests. Durable database and metadata conventions are recorded in
`ai/meadowlark.md`; the model-facing database bootstrap guide is
`ai/exploring-meadowlark.md`.

Meadowlark is no longer the active design area. Preserve its discovery,
metadata, provenance, transaction, and restart contracts, but do not continue
with another append surface unless the user explicitly returns to it.

## Next Step

No next coding step has been selected. Discuss the next area with the user
after the current checkpoint is committed. Candidate work remains in
`ai/improvements.md`, with the separately developed static-shard-builder design
in `ai/static-shards.md`.
