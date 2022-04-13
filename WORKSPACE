load("@bazel_tools//tools/build_defs/repo:git.bzl", "new_git_repository")
new_git_repository(
    name = "googletest",
    remote = "https://github.com/google/googletest",
    tag = "release-1.8.1",
    build_file = "gtest.BUILD"
)

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_file")
http_file(
    name = "nlohmann",
    sha256 = "ada63e3742009d608632e6bd4b294d9d246d88cc20b80c659f4ae75d0a053c47",
    urls = [
	"https://raw.githubusercontent.com/nlohmann/json/" +
	    "4fc98e0b34fb152497f0cd160e58c2f943b19b1e" +
	    "/single_include/nlohmann/json.hpp",
    ],
    downloaded_file_path = "json.hpp",
)
