# Cottontail Agent Notes

Be a little chill. This repo started life as a research project exploring
novel indexing structures. Now we are moving toward a more generally usable
library, along with a Python wrapper. We want the code to be solid and usable.
There is no rush. We want people to have a good experience when we release.

This file is mostly intended for an AI coding assistant working on the
development of Cottontail, but of course anyone is free to look around.

All agent documentation lives in the `ai/` directory.

## Before Doing Any Work

Read:

- `ai/architecture.md`

If it exists, also read:

- `ai/plan.md`

`ai/plan.md` records the current planned next coding step. Use it to continue
planned work instead of restarting the design discussion.

## Verification Rule

Unless the user explicitly asks for runtime experiments, ranking runs, evals,
or benchmarks, only run compile/build checks.

## Files In `ai/`

### `ai/architecture.md`

Overview of the core architecture. This file is authoritative and should not be
modified unless explicitly authorized. You may, however, suggest when changes
are appropriate.

### `ai/plan.md`

Current plan for the next coding step, when one has been materialized.

### `ai/notes.md`

Working notes about the repository structure, modules, and behavior. Keep notes
concise and factual.

### `ai/log.md`

Change log. Record significant actions performed in the repository. Each entry
should include a timestamp in ISO 8601 format and a short description.

## Modification Rules

- Do not modify `ai/architecture.md` unless explicitly requested.
- You may update other files in `ai/` as needed while working.
- Keep `ai/notes.md` concise and factual.
- Keep `ai/log.md` as the timestamped record of significant actions.
