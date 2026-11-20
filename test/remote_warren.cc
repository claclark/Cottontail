#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include "gtest/gtest.h"

#include "meadowlark/meadowlark.h"
#include "meadowlark/remote_warren.h"
#include "meadowlark/server.h"

namespace {

using cottontail::addr;
using cottontail::maxfinity;
using cottontail::meadowlark::RemoteWarren;

class TestServer {
public:
  bool start() {
    const char *tmp = std::getenv("TEST_TMPDIR");
    std::string directory =
        std::string(tmp == nullptr ? "/tmp" : tmp) + "/remote-warren-XXXXXX";
    if (mkdtemp(directory.data()) == nullptr)
      return false;
    directory_ = directory;
    meadow_ = directory_ + "/test.meadow";
    child_ = fork();
    if (child_ == 0) {
      std::string error;
      bool created =
          cottontail::meadowlark::create_meadow(meadow_, &error) != nullptr;
      if (created && cottontail::meadowlark::server(meadow_, &error))
        _exit(0);
      std::fprintf(stderr, "%s\n", error.c_str());
      _exit(1);
    }
    return child_ > 0;
  }

  std::shared_ptr<RemoteWarren> connect(std::string *error) {
    using namespace std::chrono_literals;
    auto deadline = std::chrono::steady_clock::now() + 5s;
    *error = "Server has not published an endpoint";
    do {
      std::ifstream endpoint(meadow_ + "/meadowlark.url");
      std::string url;
      int port;
      if (endpoint >> url &&
          std::sscanf(url.c_str(), "http://127.0.0.1:%d/", &port) == 1) {
        auto warren = RemoteWarren::make(port, error);
        if (warren != nullptr)
          return warren;
      }
      std::this_thread::sleep_for(20ms);
    } while (std::chrono::steady_clock::now() < deadline);
    return nullptr;
  }

  void stop() {
    if (child_ > 0) {
      kill(child_, SIGTERM);
      while (waitpid(child_, nullptr, 0) < 0 && errno == EINTR) {
      }
      child_ = -1;
    }
  }

  ~TestServer() {
    stop();
    if (!directory_.empty()) {
      std::error_code error;
      std::filesystem::remove_all(directory_, error);
    }
  }

private:
  pid_t child_ = -1;
  std::string directory_, meadow_;
};

} // namespace

TEST(RemoteWarren, SessionsAndDisconnect) {
  TestServer server;
  ASSERT_TRUE(server.start());
  std::string error;
  std::shared_ptr<cottontail::Warren> reader = server.connect(&error);
  ASSERT_NE(reader, nullptr) << error;
  ASSERT_NE(reader->tokenizer(), nullptr);
  reader->start();
  auto writer = reader->clone(&error);
  ASSERT_NE(writer, nullptr) << error;
  EXPECT_FALSE(writer->started());

  ASSERT_TRUE(writer->transaction(&error)) << error;
  addr p, q;
  ASSERT_TRUE(writer->appender()->append("remote café\n", &p, &q, &error))
      << error;
  ASSERT_NE(writer->featurizer(), nullptr);
  const addr feature = writer->featurizer()->featurize("probe:remote");
  constexpr addr payload = cottontail::minfinity + 42;
  ASSERT_TRUE(writer->annotator()->annotate(feature, p, q, payload, &error))
      << error;
  ASSERT_TRUE(writer->ready(&error)) << error;
  writer->commit();

  EXPECT_EQ(reader->txt()->tokens(), 0);
  reader->end();
  reader->start();
  auto text = reader->hopper_from_gcl("remote", &error);
  ASSERT_NE(text, nullptr) << error;
  text->tau(0, &p, &q);
  ASSERT_LT(p, maxfinity);
  EXPECT_NE(reader->txt()->translate(p, p + 1).find("remote café"),
            std::string::npos);
  text.reset();

  auto packed = reader->idx()->hopper(feature);
  ASSERT_NE(packed, nullptr);
  reader.reset();
  addr value;
  packed->tau(0, &p, &q, &value);
  ASSERT_LT(p, maxfinity);
  EXPECT_EQ(value, payload);

  server.stop();
  addr cached_p, cached_q;
  packed->tau(0, &cached_p, &cached_q);
  EXPECT_EQ(cached_p, p);
  EXPECT_EQ(cached_q, q);
  packed->tau(p + 1, &p, &q);
  EXPECT_EQ(p, maxfinity);
  EXPECT_EQ(q, maxfinity);
  EXPECT_FALSE(writer->transaction(&error));
  EXPECT_NE(error.find("connection lost"), std::string::npos) << error;
}
