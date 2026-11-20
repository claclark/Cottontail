# Current Plan

## Working Rule

No coding without a concrete reason, discussion, and explicit confirmation
from the user. Documentation review or approval of one narrow change does not
authorize adjacent source changes.

Work on one agreed step at a time. Approval for one step does not authorize
starting a later step. Entries in `ai/improvements.md` are possibilities, not
active work, until the user promotes one into this plan.

If an agreed change appears to require a broader abstraction or materially
different design, stop and discuss it. Proposals for cleanup and refactoring
are welcome, but they also require discussion before implementation.

For the current Meadowlark push, the user makes all commits and runs the
regression tests. Agent verification is compile/build-only unless the user
explicitly requests a runtime experiment or test run.

When an agreed step is complete, move its checkpoint to
[completed.md](completed.md). Keep verification status with the checkpoint.

## Active Direction: Ship Meadowlark

Shipping Meadowlark remains the priority. The Python wrapper has its own
repository and release work. No new Cottontail coding step has been selected.

Use [completed.md](completed.md) for the milestones behind this release and
[improvements.md](improvements.md) for possible follow-ups. See
[meadowlark.md](meadowlark.md) for the current ingestion and restart contracts
and [memory.md](memory.md) for deferred server memory work.

## Parked Regexp Work

- Continue normal dogfooding during Meadowlark work. Once the collection is
  useful, return to indexed regexp execution, then add useful follow/union
  special cases to both the index and scanner.
- Keep the existing matcher and runner design while collecting workloads.
- Dictionary-backed short matching, Warren phrase/regexp entry points,
  cross-line code ingestion, and exhaustive indexed literal testing remain
  unfinished. Their place in the regexp return plan is in regex.md.
- The frontier-based seeking discussion is preliminary, not a proved general
  algorithm. No follow decomposition or general springy-NFA runner was added.

Current implementation: [cgrep.md](cgrep.md).
Indexed foundations, return plan, and design questions: [regex.md](regex.md).
Search-tool limitations: [cgrep-improvements.md](cgrep-improvements.md).
Historical changes: [log.md](log.md).
