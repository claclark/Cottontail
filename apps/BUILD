load("@rules_cc//cc:defs.bzl", "cc_binary")

cc_binary(
    name = "containers",
    srcs = [
      "containers.cc",
    ],
    deps = [
      "//src:cottontail",
    ],
    linkopts = [
      "-pthread",
    ],
)

cc_binary(
    name = "cgrep",
    srcs = [
      "cgrep.cc",
    ],
    deps = [
      "//src:cottontail",
    ],
    visibility = [
      "//test:__pkg__",
    ],
)

cc_binary(
    name = "fast-id",
    srcs = [
      "fast-id.cc",
    ],
    deps = [
      "//src:cottontail",
    ], 
    linkopts = [
      "-pthread",
    ],
)

cc_binary(
    name = "fiver2hazel",
    srcs = [
      "fiver2hazel.cc",
    ],
    deps = [
      "//src:cottontail",
    ],
    linkopts = [
      "-pthread",
    ],
)

cc_binary(
    name = "consolidate",
    srcs = [
      "consolidate.cc",
    ],
    deps = [
      "//src:cottontail",
    ],
    linkopts = [
      "-pthread",
    ],
)

genrule(
    name = "finish-merging-compat",
    srcs = [
        ":consolidate",
    ],
    outs = [
        "finish-merging",
    ],
    cmd = "ln -s consolidate $@",
    executable = True,
)

cc_binary(
    name = "fluffy",
    srcs = [
      "fluffy.cc",
    ],
    deps = [
      "//src:cottontail",
      "//third_party/linenoise",
    ],
    linkopts = [
      "-pthread"
    ],
    visibility = [
      "//test:__pkg__",
    ],
)

genrule(
    name = "inspect-alias",
    srcs = [
        ":fluffy",
    ],
    outs = [
        "inspect",
    ],
    cmd = "ln -s fluffy $@",
    executable = True,
)

cc_binary(
    name = "forage",
    srcs = [
      "forage.cc",
    ],
    deps = [
      "//src:cottontail",
    ],
    linkopts = [
      "-pthread",
    ],
)

cc_binary(
    name = "merge-hazels",
    srcs = [
      "merge-hazels.cc",
    ],
    deps = [
      "//src:cottontail",
    ],
    linkopts = [
      "-pthread",
    ],
)

cc_binary(
    name = "meadowlark",
    srcs = [
      "meadowlark.cc",
    ],
    deps = [
      "//src:cottontail",
    ],
    linkopts = [
      "-pthread",
    ],
    visibility = [
      "//test:__pkg__",
    ],
)

cc_binary(
    name = "parameters",
    srcs = [
      "parameters.cc",
    ],
    deps = [
      "//src:cottontail",
   ],
   linkopts = [
      "-pthread",
   ],
)

cc_binary(
    name = "rank",
    srcs = [
      "rank.cc",
    ],
    deps = [
      "//src:cottontail",
    ],
    linkopts = [
      "-pthread",
    ],
)

cc_binary(
    name = "set",
    srcs = [
      "set.cc",
    ],
    deps = [
      "//src:cottontail",
    ],
    linkopts = [
      "-pthread",
    ],
)

cc_binary(
    name = "simple",
    srcs = [
      "simple.cc",
    ],
    deps = [
      "//src:cottontail",
    ],
    linkopts = [
      "-pthread",
    ],
)

cc_binary(
    name = "splade",
    srcs = [
      "splade.cc",
    ],
    deps = [
      "//src:cottontail",
    ],
    visibility = [
      "//test:__pkg__",
    ],
)

cc_binary(
    name = "ssr-client",
    srcs = [
      "ssr-client.cc",
    ],
    deps = [
      "//src:cottontail",
      "//third_party/linenoise",
    ],
    linkopts = [
      "-pthread",
    ],
)

cc_binary(
    name = "ssr-server",
    srcs = [
      "ssr-server.cc",
    ],
    deps = [
      "//src:cottontail",
    ],
    linkopts = [
      "-pthread",
    ],
)

cc_binary(
    name = "tf-df",
    srcs = [
      "tf-df.cc",
    ],
    deps = [
      "//src:cottontail",
    ] ,
    linkopts = [
      "-pthread",
    ],
)

cc_binary(
    name = "tf-idf",
    srcs = [
      "tf-idf.cc",
    ],
    deps = [
      "//src:cottontail",
    ] ,
    linkopts = [
      "-pthread",
    ],
)

cc_binary(
    name = "utf8-tables",
    srcs = [
      "utf8-tables.cc",
    ],
    deps = [
      "//src:cottontail",
    ] ,
    linkopts = [
    ],
)
