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
    ],
    copts = [
      "-I/usr/local/include",
    ],
    linkopts = [
      "-L/usr/local/lib -lboost_system -lboost_filesystem -pthread",
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
    name = "working",
    srcs = [
      "working.cc",
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
