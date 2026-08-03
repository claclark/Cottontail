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

The Meadowlark JSONL, TSV, text, and code ingestion work is complete. Durable
database and metadata conventions are recorded in `ai/meadowlark.md`; the
model-facing database bootstrap guide is `ai/exploring-meadowlark.md`.

Restartable parallel Hazel merging and foreground Bigwig consolidation are
complete. `Bigwig::consolidate(...)` implements the offline operation,
`finish-merging` delegates to it, and the user has verified the work through
the full regression suite, large MS MARCO consolidation, repeated ranking, and
interrupted background-merge recovery. The implementation, benchmarks,
recovery behavior, and focused coverage are recorded in
`ai/consolidation.md`.

The follow-up Bigwig merge-publication cleanup is committed as `63d70b8`. The
user reports that `apps/trec-example`, the regression tests, and
`./rank.sh a.meadow` all pass after that change.

## Next Step

Unknown. There is no selected next design area or coding step. The goal is to
make Cottontail a useful general library, and the next work should be chosen
only after a fresh discussion of what best advances that goal. Do not infer a
roadmap from completed-work records or from entries in `ai/improvements.md`.

The long-running-server memory discussion was useful but became a distraction
from that broader goal. Its tentative Warren-level `trim_memory()` direction
and unresolved Hazel text-cache concurrency issue are preserved in
`ai/memory.md` for possible later return; they are not the current project.

No coding step is authorized by this plan.
