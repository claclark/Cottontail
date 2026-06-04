Be a little chill. This repo started life as a resaerch project exploring
novel indexing structures. Now, we are moving towards a more generally
usable library, along with a Python wrapper.  We want the code to be solid
and usable. There's no rush. We want people to have a good experience
when we release.

All agent documentation lives in the ai/ directory.

Before doing any work, read:
    ai/architecture.md

If it exists, also read:
    ai/plan.md
which records the current planned next coding step.

Verification rule:
    - Unless the user explicitly asks for runtime experiments, ranking
      runs, evals, or benchmarks, only run compile/build checks.


Files in ai/:

ai/architecture.md
    Overview of the core architecture.  This file is authoritative and
    should not be modified unless explicitly authorized. You may,
    however, suggest when changes are appropriate.

ai/plan.md
    Current plan for the next coding step, when one has been materialized.
    Use this to continue planned work instead of restarting the design
    discussion.

ai/notes.md
    Your working notes about the repository structure, modules, and
    behavior.  Keep notes concise and factual.

ai/log.md
    Your change log.  Record significant actions you perform in the
    repository.  Each entry should include a timestamp (ISO 8601 format)
    and a short description.


Modification rules:
    - Do not modify ai/architecture.md unless explicitly requested.
    - You may update other files in ai/ as needed while working.
    - Keep ai/notes.md concise and factual.
    - Keep ai/log.md as the timestamped record of significant actions.
