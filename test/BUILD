load("@rules_cc//cc:defs.bzl", "cc_test")

cc_test(
    name = "tests",
    srcs = glob(["**/*.cc"]) + glob(["**/*.h"]),
    deps = [
        "//src:cottontail",
        "@googletest//:gtest_main",
    ],
    linkopts = [
        "-lz",
        "-pthread",
    ],
    data = [
        "books.json",
        "ranking.txt",
        "sonnet0.txt",
        "sonnet1.txt",
        "sonnet2.txt",
        "test.tsv",
        "test0.txt",
        "test.qrels",
    ],
)
