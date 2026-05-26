load("@rules_cc//cc:defs.bzl", "cc_test")

cc_test(
    name = "tests",
    srcs = glob(["**/*.cc"], exclude = ["hazel.cc"]) + glob(["**/*.h"]),
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

cc_test(
    name = "hazel_test",
    size = "small",
    srcs = [
        "hazel.cc",
    ],
    deps = [
        "//src:cottontail",
        "@googletest//:gtest_main",
    ],
    linkopts = [
        "-lz",
        "-pthread",
    ],
)
