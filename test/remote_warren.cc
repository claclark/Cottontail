#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <thread>

#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include "gtest/gtest.h"

#include "meadowlark/meadowlark.h"
#include "meadowlark/remote_protocol.h"
#include "meadowlark/server.h"
#include "src/dna.h"
#include "src/recipe.h"
#include "src/wlock.h"

namespace {

using cottontail::addr;
using cottontail::maxfinity;

bool create_server_meadow(const std::string &meadow, std::string *error) {
  auto warren = cottontail::meadowlark::create_meadow(meadow, error);
  std::string dna;
  std::map<std::string, std::string> parameters;
  if (warren == nullptr || !warren->set_parameter("container", ":", error) ||
      !warren->set_parameter("stemmer", "porter", error) ||
      !cottontail::read_dna(warren->working(), &dna, error) ||
      !cottontail::cook(dna, &parameters, error))
    return false;
  const std::map<std::string, std::string> server = {{"active", "yes"}};
  parameters["server"] = cottontail::freeze(server);
  return cottontail::write_dna(warren->working(),
                               cottontail::freeze(parameters), error);
}

class TestServer {
public:
  bool start(bool monitor = false) {
    // Bazel's TEST_TMPDIR can be longer than a Unix socket address.
    std::string directory = "/tmp/remote-warren-XXXXXX";
    if (mkdtemp(directory.data()) == nullptr)
      return false;
    directory_ = directory;
    meadow_ = directory_ + "/test.meadow";
    child_ = fork();
    if (child_ == 0) {
      std::string error;
      if (create_server_meadow(meadow_, &error)) {
        std::ofstream legacy(meadow_ + "/meadowlark.url");
        legacy << "http://127.0.0.1:12345/\n";
        legacy.close();
        if (!legacy)
          error = "Cannot write legacy Meadowlark URL";
        else if (cottontail::meadowlark::server(meadow_, &error, monitor))
          _exit(0);
      }
      std::fprintf(stderr, "%s\n", error.c_str());
      _exit(1);
    }
    return child_ > 0;
  }

  std::shared_ptr<cottontail::Warren> connect(std::string *error) {
    using namespace std::chrono_literals;
    auto deadline = std::chrono::steady_clock::now() + 5s;
    *error = "Server has not published an endpoint";
    do {
      if (std::filesystem::is_socket(socket_path()))
        return cottontail::meadowlark::open_meadow(meadow_, error);
      std::this_thread::sleep_for(20ms);
    } while (std::chrono::steady_clock::now() < deadline);
    return nullptr;
  }

  const std::string &meadow() const { return meadow_; }

  std::string socket_path() const {
    return meadow_ + "/" + cottontail::meadowlark::SERVER_SOCKET_NAME;
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

using Seek = void (cottontail::Hopper::*)(addr, addr *, addr *, addr *);

void expect_seek(cottontail::Hopper &hopper, Seek seek, addr k, addr expected_p,
                 addr expected_q, addr expected_value) {
  addr p, q, value;
  (hopper.*seek)(k, &p, &q, &value);
  EXPECT_EQ(p, expected_p);
  EXPECT_EQ(q, expected_q);
  EXPECT_EQ(value, expected_value);
}

void check_wire_lifecycle(const std::string &path) {
  using cottontail::meadowlark::remote_protocol::local_address;
  using cottontail::meadowlark::remote_protocol::Socket;
  sockaddr_un address;
  ASSERT_TRUE(local_address(path, &address));
  auto connect = [&](Socket &socket) {
    return socket.fd() >= 0 && socket.configure() &&
           ::connect(socket.fd(), reinterpret_cast<sockaddr *>(&address),
                     sizeof(address)) == 0;
  };
  Socket invalid(::socket(AF_UNIX, SOCK_STREAM, 0));
  ASSERT_TRUE(connect(invalid));
  Socket socket(::socket(AF_UNIX, SOCK_STREAM, 0));
  ASSERT_TRUE(connect(socket));
  json reply;
  auto exchange = [&](const json &request) {
    return socket.write(request) && socket.read(&reply);
  };
  ASSERT_TRUE(socket.write_bytes(std::string_view("\0", 1)));
  ASSERT_TRUE(exchange({{"protocol", "meadowlark"}, {"version", 0}}));
  ASSERT_TRUE(reply.contains("session")) << reply;
  ASSERT_TRUE(invalid.write_bytes("G"));
  char byte;
  EXPECT_EQ(recv(invalid.fd(), &byte, 1, 0), 0);

  const json started = {{"component", "warren"}, {"method", "started"}};
  ASSERT_TRUE(exchange(started));
  EXPECT_EQ(reply, (json{{"response", false}}));
  ASSERT_TRUE(exchange({{"component", "warren"}, {"method", "start"}}));
  EXPECT_EQ(reply, (json{{"response", true}}));
  ASSERT_TRUE(exchange(started));
  EXPECT_EQ(reply, (json{{"response", true}}));

  ASSERT_TRUE(exchange({{"component", "warren"},
                        {"method", "hopper_from_gcl"},
                        {"query", "remote"}}));
  ASSERT_TRUE(reply.contains("hopper") && reply["hopper"].is_string()) << reply;
  std::string handle = reply["hopper"].get<std::string>();
  const json seek = {{"component", "hopper"},
                     {"method", "tau"},
                     {"hopper", handle},
                     {"k", "0"}};
  ASSERT_TRUE(exchange(seek));
  ASSERT_TRUE(reply.contains("response")) << reply;
  addr p;
  ASSERT_TRUE(cottontail::meadowlark::remote_protocol::addr_field(
      reply.at("response"), "p", &p));
  ASSERT_LT(p, maxfinity);

  ASSERT_TRUE(socket.write(
      {{"component", "hopper"}, {"method", "complete"}, {"hopper", handle}}));
  ASSERT_TRUE(exchange(seek));
  std::string infinity = std::to_string(maxfinity);
  EXPECT_EQ(reply, (json{{"response", {{"p", infinity}, {"q", infinity}}}}));
  ASSERT_TRUE(exchange({{"component", "warren"}, {"method", "end"}}));
  EXPECT_EQ(reply, (json{{"response", true}}));
  ASSERT_TRUE(exchange(started));
  EXPECT_EQ(reply, (json{{"response", false}}));
}

} // namespace

TEST(RemoteWarren, MonitorSyncsText) {
  using namespace std::chrono_literals;
  TestServer server;
  ASSERT_TRUE(server.start(true));
  std::string error;
  auto reader = server.connect(&error);
  ASSERT_NE(reader, nullptr) << error;
  auto directory = std::filesystem::path(server.meadow()).parent_path() / "sub";
  std::filesystem::create_directory(directory);
  auto source = directory / "message.json";
  const std::string name = "./sub/message.json";

  auto wait_for = [&](const std::string &path, const std::string &expected) {
    auto deadline = std::chrono::steady_clock::now() + 12s;
    do {
      reader->start();
      auto hopper = reader->hopper_from_gcl(
          expected.empty() ? path : "(<< : " + path + ")",
          &error);
      addr p, q;
      bool found = false;
      if (hopper != nullptr) {
        hopper->tau(cottontail::minfinity + 1, &p, &q);
        if (expected.empty()) {
          found = p == maxfinity;
        } else if (p != maxfinity &&
                   reader->txt()->translate(p, q).starts_with(expected)) {
          hopper->tau(p + 1, &p, &q);
          found = p == maxfinity;
        }
      }
      reader->end();
      if (found)
        return true;
      std::this_thread::sleep_for(100ms);
    } while (std::chrono::steady_clock::now() < deadline);
    return false;
  };

  std::ofstream(source) << "first version";
  ASSERT_TRUE(wait_for(name, "first version")) << error;
  std::ofstream(source) << "second version";
  ASSERT_TRUE(wait_for(name, "second version")) << error;

  auto temporary = directory / "message.tmp";
  const std::string temp_name = "./sub/message.tmp";
  std::ofstream(temporary) << "editor save";
  // Let the temporary file reach the index so its removal is observable.
  ASSERT_TRUE(wait_for(temp_name, "editor save")) << error;
  std::filesystem::rename(temporary, source);
  ASSERT_TRUE(wait_for(name, "editor save")) << error;
  ASSERT_TRUE(wait_for(temp_name, "")) << error;

  ASSERT_TRUE(std::filesystem::remove(source));
  ASSERT_TRUE(wait_for(name, "")) << error;
}

TEST(RemoteWarren, SessionsAndDisconnect) {
  TestServer server;
  ASSERT_TRUE(server.start());
  std::string error;
  std::shared_ptr<cottontail::Warren> reader = server.connect(&error);
  ASSERT_NE(reader, nullptr) << error;
  EXPECT_EQ(reader->name(), "remote");
  ASSERT_NE(reader->tokenizer(), nullptr);
  std::string format;
  ASSERT_TRUE(reader->get_parameter("format", &format, &error)) << error;
  EXPECT_EQ(format, "meadowlark");
  EXPECT_FALSE(reader->set_parameter("format", "other", &error));
  auto writer = reader->clone(&error);
  ASSERT_NE(writer, nullptr) << error;
  EXPECT_FALSE(writer->started());
  reader->start();
  addr p, q;
  EXPECT_FALSE(reader->txt()->range(&p, &q));
  EXPECT_EQ(reader->default_container(), ":");
  ASSERT_NE(reader->stemmer(), nullptr);
  EXPECT_EQ(reader->stemmer()->stem("running"), "porter:run");
  ASSERT_TRUE(writer->transaction(&error)) << error;
  ASSERT_TRUE(writer->appender()->append("remote café\n", &p, &q, &error))
      << error;
  ASSERT_NE(writer->featurizer(), nullptr);
  auto featurizer = writer->featurizer();
  EXPECT_EQ(featurizer->name(), "json");
  std::string wrapped_name, wrapped_recipe;
  ASSERT_TRUE(cottontail::unwrap(featurizer->recipe(), &wrapped_name,
                                 &wrapped_recipe, &error))
      << error;
  EXPECT_EQ(wrapped_name, "hashing");
  EXPECT_TRUE(wrapped_recipe.empty());
  const addr short_feature = featurizer->featurize("café");
  EXPECT_EQ(featurizer->translate(short_feature), "café");
  const addr feature = featurizer->featurize("probe:remote");
  ASSERT_GT(feature, cottontail::null_feature);
  constexpr addr payload = cottontail::minfinity + 42;
  ASSERT_TRUE(writer->annotator()->annotate(feature, p, q, payload, &error))
      << error;
  ASSERT_TRUE(writer->ready(&error)) << error;
  writer->commit();

  EXPECT_EQ(reader->txt()->tokens(), 0);
  auto snapshot = reader->clone(&error);
  ASSERT_NE(snapshot, nullptr) << error;
  ASSERT_TRUE(snapshot->started());
  EXPECT_EQ(snapshot->txt()->tokens(), 0);

  reader->end();
  reader->start();
  EXPECT_GT(reader->txt()->tokens(), 0);
  EXPECT_EQ(reader->idx()->count(feature), 1);
  EXPECT_GT(reader->idx()->vocab(), 0);
  EXPECT_EQ(snapshot->txt()->tokens(), 0);
  snapshot->end();
  snapshot.reset();
  auto text = reader->hopper_from_gcl("remote", &error);
  ASSERT_NE(text, nullptr) << error;
  text->tau(0, &p, &q);
  ASSERT_LT(p, maxfinity);
  addr first, last;
  ASSERT_TRUE(reader->txt()->range(&first, &last));
  EXPECT_LE(first, p);
  EXPECT_GE(last, p + 1);
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
  using cottontail::Hopper;
  expect_seek(*packed, &Hopper::rho, q, p, q, payload);
  expect_seek(*packed, &Hopper::uat, q, p, q, payload);
  expect_seek(*packed, &Hopper::ohr, p, p, q, payload);
  EXPECT_EQ(packed->L(q), p);
  EXPECT_EQ(packed->R(p), q);
  check_wire_lifecycle(server.socket_path());

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
  EXPECT_EQ(featurizer->featurize("probe:remote"), cottontail::null_feature);
  EXPECT_TRUE(featurizer->translate(short_feature).empty());
}

TEST(RemoteWarren, RecreateWithStaleSocket) {
  TestServer server;
  ASSERT_TRUE(server.start());
  std::string error;
  ASSERT_NE(server.connect(&error), nullptr) << error;
  EXPECT_FALSE(std::filesystem::exists(server.meadow() + "/meadowlark.url"));
  EXPECT_EQ(cottontail::WLock::recreate("bigwig", server.meadow(), &error),
            nullptr);
  EXPECT_TRUE(std::filesystem::is_socket(server.socket_path()));
  server.stop();
  ASSERT_TRUE(std::filesystem::is_socket(server.socket_path()));
  ASSERT_NE(cottontail::WLock::recreate("bigwig", server.meadow(), &error),
            nullptr)
      << error;
  EXPECT_FALSE(std::filesystem::exists(server.socket_path()));
}

TEST(RemoteWarren, EraseAndAbort) {
  TestServer server;
  ASSERT_TRUE(server.start());
  std::string error;
  auto warren = server.connect(&error);
  ASSERT_NE(warren, nullptr) << error;

  addr p, q;
  ASSERT_TRUE(warren->transaction(&error)) << error;
  ASSERT_TRUE(warren->appender()->append("persistent text\n", &p, &q, &error))
      << error;
  ASSERT_TRUE(warren->ready(&error)) << error;
  warren->commit();
  warren->start();
  ASSERT_TRUE(warren->txt()->range(&p, &q));
  warren->end();

  ASSERT_TRUE(warren->transaction(&error)) << error;
  ASSERT_TRUE(warren->annotator()->erase(p, q, &error)) << error;
  ASSERT_TRUE(warren->ready(&error)) << error;
  warren->abort();
  warren->start();
  EXPECT_NE(warren->txt()->translate(p, q).find("persistent text"),
            std::string::npos);
  warren->end();

  ASSERT_TRUE(warren->transaction(&error)) << error;
  ASSERT_TRUE(warren->annotator()->erase(p, q, &error)) << error;
  ASSERT_TRUE(warren->ready(&error)) << error;
  warren->commit();
  warren->start();
  EXPECT_TRUE(warren->txt()->translate(p, q).empty());
  auto deleted = warren->hopper_from_gcl("persistent", &error);
  ASSERT_NE(deleted, nullptr) << error;
  deleted->tau(0, &p, &q);
  EXPECT_EQ(p, maxfinity);
  EXPECT_EQ(q, maxfinity);
  warren->end();
}
