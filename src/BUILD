cc_library(
    name = "cottontail",
    srcs = glob(["*.cc"]),  
    hdrs = glob(["*.h"]),
    deps = ["@nlohmann_json//:json"],
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
