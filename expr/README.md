# Legacy Applications and Experiments

Applications from older experiments and tests, preserved as records of that
work. These include TREC CAsT and Deep Learning experiments, specialized corpus
builders, the earlier SPLADE tools, timing and dynamic-update tests, and
supporting scripts and queries.

Some programs retain experiment-specific paths, data layouts, and parameters.

Build from the repository root with `bazel build //expr:all`. Compiled programs
are placed in `bazel-bin/expr/`.

Current utilities live in [apps/](../apps/). The two examples from the paper
are collected separately in [iirj/](../iirj/).
