cc_test(
    name = "tests",
    srcs = glob(["**/*.cc"]),
    deps = [
        "//src:cottontail",
        "@googletest//:gtest_main",
    ],
    copts = [
        "'-std=c++14'",
    ],
    linkopts = [
        "-lz -pthread"
    ],
    data = [
        "test0.txt",
        "ranking.txt",
        "test.qrels",
    ],
)
