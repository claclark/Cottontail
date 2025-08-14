cc_library(
    name = "cottontail",
    srcs = glob(["*.cc"]) + ["//meadowlark:meadowlark_cc"],
    hdrs = glob(["*.h"])  + ["//meadowlark:meadowlark_h"],
    deps = [
        "@nlohmann_json//:json",
    ],
    copts = [
        "-std=c++14",
    ],
    linkopts = [
        "-lz",
        "-lpthread",
    ],
    visibility = [
        "//visibility:public",
    ],
)
