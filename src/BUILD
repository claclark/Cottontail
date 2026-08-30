load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "cottontail",
    srcs = glob(["*.cc"]) + [
        "//gcl:gcl_cc",
        "//meadowlark:meadowlark_cc",
        "//regexp:regexp_cc",
    ],
    hdrs = glob(["*.h"]) + [
        "//gcl:gcl_h",
        "//meadowlark:meadowlark_h",
        "//regexp:regexp_h",
    ],
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
