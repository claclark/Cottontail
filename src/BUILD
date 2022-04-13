cc_library(
    name = "cottontail",
    srcs = glob(["*.cc"]) + ["@nlohmann//file"],  
    hdrs = glob(["*.h"]),
    copts = [
      "'-std=c++14'",
    ],
    linkopts = [
      "-lz",
    ],
    visibility = [
      "//visibility:public",
    ],
)
