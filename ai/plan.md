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

The next Cottontail diffs support shipping Meadowlark through the Python
wrapper. The wrapper has its own repository and release work.

The managed tree is an agent's workspace. A downloader, converter, or script
can read and write ordinary files without knowing about Meadowlark. The live
index gives agents a memory they can query across those files and across
sessions. A Git pull that changes the working tree feeds the same monitor.
An independent watchdog could periodically run `git pull --ff-only` in a
GitHub checkout. Plan to exclude `.git/` from indexing and `.meadow/` from
version control.

1. **Manage a directory.** `meadowlark --manage DIR` creates the directory
   and its `.meadow` as needed, configures monitoring, and starts it
   immediately. Running it again reuses the meadow and any running server.
2. **Ignore rules.** Support `.gitignore` syntax and keep the active meadow
   unconditionally excluded. Decide where to read rules (including a possible
   `.meadow/meadowlark.ignore`) and which layer handles changes to them.
   Editing a rule can change eligibility throughout an otherwise idle subtree.
3. **Startup sync.** Reconcile managed paths added, changed, or removed while
   the server was down. Explore a meadow-wide timestamp to avoid retaining
   per-file timestamps. One candidate is the start time of the last completed
   reconciliation. A shard's mtime can advance while a file is still queued,
   and a copied file can retain an old mtime. The choice is open. Limit the
   reconciliation to filesystem-managed records.
4. **File types.** Check monitored input for UTF-8 text and choose an append
   adapter. Consider treating embedded NUL as binary even in valid UTF-8.
   Settle recognition and replacement behavior in that diff.
5. **Tagged JSON.** `append_tagged(tag, json, ...)` stores an object directly
   in the meadow under a feature such as `#claim`, without a filesystem file.
   On first use, store an `@` record with the tag name and the first object's
   shape in the same transaction. Later objects can have different shapes.
   The intended discovery query is `#` for tag descriptions and `#claim` for
   the objects carrying that tag. A feature count may suffice to detect first
   use. Work out how to distinguish a known tag with no remaining objects.
   Bare `#` already marks code lines in existing meadows, so its new use needs
   a compatibility decision.
6. **Idle server lifecycle.** Explore trimming memory while the watcher stays
   active, or letting the server exit after the last client leaves and full
   consolidation finishes. Python's next open can launch the configured server,
   with startup sync covering changes made while it was stopped. A proposed
   flag on open would convey the caller's preference to a new or running
   server. Delivery of that preference and behavior with multiple clients
   remain open.

### Python workflow

The sibling `../Meadowlark` prototype binds the Warren query operations.
`Meadow.open(path)` opens a Warren and `with meadow:` brackets one read
snapshot. Within it, `meadow.hopper(gcl)` yields `(p, q, value)` and supports
`tau`, `rho`, `uat`, and `ohr`. Use `meadow.translate(p, q)` to read a hit.
A fresh snapshot can see subsequent commits.

Keep that query surface available alongside BM25, requests to forage, and
direct insertion of a tagged JSON object. Python method names are provisional.
Agents can write large inputs into the managed tree and query them once the
monitor commits them. When the wrapper settles, give the workspace a short
`README.md` for a newly arrived agent. Show how to open it, discover files
with `/`, discover tag shapes with `#`, and retrieve a result. The meadow
itself supplies the current inventory.

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
