# Goal: Plan Hazel Merge Testing

Plan behavioral and regression testing for the existing Hazel-to-Hazel merge
implementation, then implement tests only after the user explicitly approves
the plan.

## Current Status

- A first Working-based `Hazel::merge(...)` implementation exists in
  `src/hazel.*`.
- The implementation is documented in `ai/hazel.md`.
- It has been compile-checked, including:
  - `bazel build //apps:fiver2hazel //apps:working`
  - `bazel build //...`
- Behavioral/regression testing has not yet been designed or implemented.

## Important Constraint

Do not write test code or implementation code without explicit user approval.
The next step is planning and review only.

Also follow the top-level verification rule: run compile/build checks only
unless the user explicitly asks for runtime experiments, ranking runs, evals,
or benchmarks.

## Planning Targets

A test plan might include these ideas, but talk to the user first:

- merging two small Hazel files with disjoint text/address ranges;
- preserving DNA and owner Warren `parameters` semantics where intended;
- validating idx posting lookup after merge for inline and non-inline postings;
- validating txt translation across chunk boundaries and across input files;
- confirming feature counts/vocabulary behavior after merge;
- handling deletion/null-feature semantics if already supported by the merge;
- rejecting incompatible Hazel inputs with clear errors;
- checking separator-newline handling from `Fiver::hazel(...)`;
- identifying which checks belong in automated unit tests versus user-guided
  local scripts.

## Starting Points

Review these before planning:

- `ai/architecture.md`
- `ai/hazel.md`
- `ai/hazel-testing.md` if relevant
- `src/hazel.h`
- `src/hazel.cc`
- `src/fiver.cc`
- existing tests under `test/`

After review, talk to the user.
