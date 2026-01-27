load("@rules_cc//cc:defs.bzl", "cc_library", "cc_binary")

cc_library(
  name = "collection",
  srcs = [
    "collection.cc",
  ],
  hdrs = [
    "collection.h",
  ],
  deps = [
    "//src:cottontail",
  ],
)

cc_library(
  name = "walk",
  srcs = [
    "walk.cc",
  ],
  hdrs = [
    "walk.h",
  ],
  deps = [
  ],
)

cc_binary(
    name = "c4-build",
    srcs = [
      "c4-build.cc",
    ],
    deps = [
      "//src:cottontail",
      "collection",
    ],
    linkopts = [
      "-pthread",
    ],
)


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
    name = "dagstuhl",
    srcs = [
      "dagstuhl.cc",
    ],
    deps = [
      "//src:cottontail",
    ],
    linkopts = [
      "-pthread",
    ],
)

cc_binary(
    name = "dagstuhl-trec45",
    srcs = [
      "dagstuhl-trec45.cc",
    ],
    deps = [
      "//src:cottontail",
    ],
    linkopts = [
      "-pthread",
    ],
)

cc_binary(
    name = "dynamic-test",
    srcs = [
      "dynamic-test.cc",
    ],
    deps = [
      "//src:cottontail",
      "walk",
    ],
    linkopts = [
      "-L/usr/local/lib -lboost_system -lboost_filesystem -pthread",
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
    name = "fluffy",
    srcs = [
      "fluffy.cc",
    ],
    deps = [
      "//src:cottontail",
    ],
    linkopts = [
      "-lreadline -pthread"
    ],
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
    name = "idx-timing",
    srcs = [
      "idx-timing.cc",
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
)

cc_binary(
    name = "mt",
    srcs = [
      "mt.cc",
    ],
    deps = [
      "//src:cottontail",
    ],
    linkopts = [
      "-lreadline -pthread",
    ],
)

cc_binary(
    name = "parameters",
    srcs = [
      "parameters.cc",
    ],
    copts = [
      "'-std=c++14'",
    ],
    deps = [
      "//src:cottontail",
   ],
   linkopts = [
      "-pthread",
   ],
)

cc_binary(
    name = "pref2train",
    srcs = [
      "pref2train.cc",
    ],
    deps = [
      "//src:cottontail",
    ],
    copts = [
      "'-std=c++14'",
    ],
    linkopts = [
      "-lreadline -pthread"
    ],
)

cc_binary(
    name = "qap",
    srcs = [
      "qap.cc",
    ],
    deps = [
      "//src:cottontail",
      "collection",
    ],
    copts = [
      "'-std=c++14'",
    ],
    linkopts = [
      "-pthread",
    ],
)

cc_binary(
    name = "rag2024-extract",
    srcs = [
      "rag2024-extract.cc",
    ],
    deps = [
      "//src:cottontail",
    ],
    copts = [
      "'-std=c++14'",
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
    copts = [
      "'-std=c++14'",
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
      "collection",
      "walk",
    ],
    copts = [
      "-I/usr/local/include",
    ],
    linkopts = [
      "-L/usr/local/lib -lboost_system -lboost_filesystem -pthread",
    ],
)

cc_binary(
    name = "splade-annotate",
    srcs = [
      "splade-annotate.cc",
    ],
    deps = [
      "//src:cottontail",
    ] ,
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
    copts = [
      "'-std=c++14'",
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
    copts = [
      "'-std=c++14'",
    ],
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
    name = "trec-example",
    srcs = [
      "trec-example.cc",
    ],
    deps = [
      "//src:cottontail",
      "walk",
    ],
    copts = [
      "'-std=c++20'",
    ],
    linkopts = [
      "-L/usr/local/lib -lboost_system -lboost_filesystem -pthread",
    ],
)

cc_binary(
    name = "treccast20",
    srcs = [
      "treccast20.cc",
    ],
    deps = [
      "//src:cottontail",
      "collection",
    ],
    copts = [
      "'-std=c++14'",
    ],
    linkopts = [
      "-pthread",
    ],
)

cc_binary(
    name = "treccast20-build",
    srcs = [
      "treccast20-build.cc",
    ],
    deps = [
      "//src:cottontail",
      "collection",
    ],
    linkopts = [
      "-pthread",
    ],
)

cc_binary(
    name = "treccast21",
    srcs = [
      "treccast21.cc",
    ],
    deps = [
      "//src:cottontail",
      "collection",
    ],
    copts = [
      "'-std=c++14'",
    ],
    linkopts = [
      "-pthread",
    ],
)

cc_binary(
    name = "treccast21-build",
    srcs = [
      "treccast21-build.cc",
    ],
    deps = [
      "//src:cottontail",
      "collection",
    ],
    linkopts = [
      "-pthread",
    ],
)

cc_binary(
    name = "treccast22",
    srcs = [
      "treccast22.cc",
    ],
    deps = [
      "//src:cottontail",
      "collection",
    ],
    copts = [
      "'-std=c++14'",
    ],
    linkopts = [
      "-pthread",
    ],
)

cc_binary(
    name = "treccast22-build",
    srcs = [
      "treccast22-build.cc",
    ],
    deps = [
      "//src:cottontail",
      "collection",
    ],
    linkopts = [
      "-pthread",
    ],
)

cc_binary(
    name = "trecdl21",
    srcs = [
      "trecdl21.cc",
    ],
    deps = [
      "//src:cottontail",
      "collection",
    ],
    copts = [
      "'-std=c++14'",
    ],
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

cc_binary(
    name = "wikidata-build",
    srcs = [
      "wikidata-build.cc",
    ],
    deps = [
      "//src:cottontail",
    ] ,
    linkopts = [
    ],
)

cc_binary(
    name = "working",
    srcs = [
      "working.cc",
    ],
    deps = [
      "//src:cottontail",
      "collection",
    ],
    copts = [
      "'-std=c++14'",
    ],
    linkopts = [
      "-lreadline -pthread",
    ],
)
