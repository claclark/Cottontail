load("@rules_cc//cc:defs.bzl", "cc_test")
load("@rules_shell//shell:sh_test.bzl", "sh_test")

cc_test(
    name = "tests",
    srcs = glob(["**/*.cc"], exclude = [
        "cgrep.cc",
        "hazel.cc",
        "nfa.cc",
        "optimizer.cc",
    ]) + glob(["**/*.h"]),
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
        "code.txt",
        "ranking.txt",
        "sonnet0.txt",
        "sonnet1.txt",
        "sonnet2.txt",
        "test.tsv",
        "test0.txt",
        "tokenless.txt",
        "test.qrels",
    ],
)

cc_test(
    name = "cgrep_test",
    size = "small",
    srcs = [
        "cgrep.cc",
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

sh_test(
    name = "cgrep_app_test",
    size = "small",
    srcs = [
        "cgrep.sh",
    ],
    data = [
        "//apps:cgrep",
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

cc_test(
    name = "nfa_test",
    size = "small",
    srcs = [
        "nfa.cc",
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

cc_test(
    name = "optimizer_test",
    size = "small",
    srcs = [
        "optimizer.cc",
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
