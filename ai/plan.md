# Current Plan

No coding without discussion and confirmation from the user.

Finish Meadowlark.

Next work, in no particular order:

- Do more testing of dynamic update.
- Add restart to Meadowlark by checking what is already in the database before
  appending.
- Implement proper commit behavior for multiple Meadowlark clones.
  - `Warren::commit_all(...)` owns fallback. It dispatches to
    `Bigwig::commit_all(...)` only for vectors whose entries are all named
    `bigwig` and cast to `Bigwig`.
  - `Bigwig::commit_all(...)` should attempt the future atomic path only for
    Bigwigs sharing the same literal `Working` object. If it cannot do the
    atomic commit, it returns `false` and Warren-level fallback commits each
    Warren individually.
- Delete `mrg.*` files during sanitization when appropriate.
- Turn `apps/gcl-timing` into a server-like passage/query tool.

Discuss the next item with the user before coding when the choice or scope is
not already explicit.
