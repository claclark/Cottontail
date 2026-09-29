load("@rules_cc//cc:defs.bzl", "cc_binary")

cc_binary(
    name = "json-examples",
    srcs = [
      "json-examples.cc",
    ],
    deps = [
      "//src:cottontail",
    ],
    linkopts = [
      "-pthread"
    ],
)

cc_binary(
    name = "jsonl",
    srcs = [
      "jsonl.cc",
    ],
    deps = [
      "//src:cottontail",
    ],
    linkopts = [
      "-pthread"
    ],
)

cc_binary(
    name = "trec-example",
    srcs = [
      "trec-example.cc",
    ],
    deps = [
      "//src:cottontail",
    ],
    linkopts = [
      "-pthread",
    ],
)
