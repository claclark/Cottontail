load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "cottontail",
    srcs = glob(["*.cc"]) + ["//meadowlark:meadowlark_cc"],
    hdrs = glob(["*.h"])  + ["//meadowlark:meadowlark_h"],
    deps = [
        "@nlohmann_json//:json",
    ],
    linkopts = [
        "-lz",
        "-lpthread",
    ],
    visibility = [
        "//visibility:public",
    ],
)
