cc_test(
    name = "tests",
    srcs = glob(["**/*.cc"]) + glob(["**/*.h"]),
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
        "books.json",
        "ranking.txt",
        "sonnet0.txt",
        "sonnet1.txt",
        "sonnet2.txt",
        "test0.txt",
        "test.qrels",
    ],
)
