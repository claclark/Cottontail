cc_library(
    name = "cottontail",
    srcs = glob([
        "*.cc",
        "meadowlark/*.cc",
    ]),
    hdrs = glob([
        "*.h",
        "meadowlark/*.h",
    ]),
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
