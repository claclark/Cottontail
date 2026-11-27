#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "gtest/gtest.h"
#include "src/monitor.h"

namespace {
namespace fs = std::filesystem;
using namespace std::chrono_literals;

class MonitorTest : public ::testing::Test {
protected:
  void SetUp() override {
    const char *tmp = std::getenv("TEST_TMPDIR");
    fs::path base = tmp ? fs::path(tmp) : fs::temp_directory_path();
    std::string pattern = (base / "monitor-XXXXXX").string();
    ASSERT_NE(mkdtemp(pattern.data()), nullptr);
    temporary_ = pattern;
    root_ = temporary_ / "watched";
    fs::create_directory(root_);
  }

  void start(const std::vector<std::string> &ignore = {}) {
    std::string error;
    monitor_ = cottontail::Monitor::make(root_.string(), ignore, &error);
    ASSERT_NE(monitor_, nullptr) << error;
    reader_ = std::thread([this] {
      for (auto path = monitor_->next(); !path.empty();
           path = monitor_->next()) {
        std::lock_guard lock(mutex_);
        paths_.insert(path);
        changed_.notify_one();
      }
    });
  }

  void expect_paths(std::multiset<std::string> expected) {
    std::unique_lock lock(mutex_);
    changed_.wait_for(lock, 8s,
                      [&] { return paths_.size() >= expected.size(); });
    EXPECT_EQ(paths_, expected);
    paths_.clear();
  }

  void expect_quiet() {
    std::unique_lock lock(mutex_);
    EXPECT_FALSE(changed_.wait_for(lock, 1500ms, [&] { return !paths_.empty(); }))
        << ::testing::PrintToString(paths_);
  }

  void TearDown() override {
    if (monitor_)
      monitor_->stop();
    if (reader_.joinable())
      reader_.join();
    if (!temporary_.empty())
      fs::remove_all(temporary_);
  }

  fs::path temporary_, root_;

private:
  std::shared_ptr<cottontail::Monitor> monitor_;
  std::thread reader_;
  std::mutex mutex_;
  std::condition_variable changed_;
  std::multiset<std::string> paths_;
};

TEST_F(MonitorTest, InitialInventoryIsSilent) {
  fs::create_directories(root_ / "sub");
  std::ofstream(root_ / "sub/existing") << "old";
  ASSERT_NO_FATAL_FAILURE(start({"/.meadow"}));
  fs::create_directory(root_ / "empty");
  fs::create_directory(root_ / ".meadow");
  std::ofstream(root_ / ".meadow/private") << "hidden";
  fs::create_symlink(root_ / "sub/existing", root_ / "link");
  expect_quiet();

  std::ofstream(root_ / "sub/existing") << "edited";
  std::ofstream(root_ / "added") << "new";
  expect_paths({"/added", "/sub/existing"});
  fs::remove(root_ / "sub/existing");
  expect_paths({"/sub/existing"});
}

TEST_F(MonitorTest, RenamePopulatedDirectory) {
  fs::create_directories(root_ / "old/child");
  fs::create_directories(root_ / "old/.hidden");
  fs::create_directories(root_ / "older");
  std::ofstream(root_ / "old/a");
  std::ofstream(root_ / "old/child/b");
  std::ofstream(root_ / "old/.hidden/c");
  std::ofstream(root_ / "older/unrelated");
  fs::create_directory_symlink(root_ / "older", root_ / "old/link");
  ASSERT_NO_FATAL_FAILURE(start());

  fs::rename(root_ / "old", root_ / "new");
  expect_paths({"/new/a", "/new/child/b", "/new/.hidden/c",
                "/old/a", "/old/child/b", "/old/.hidden/c"});
  fs::rename(root_ / "new", temporary_ / "outside");
  expect_paths({"/new/a", "/new/child/b", "/new/.hidden/c"});
}

TEST_F(MonitorTest, MoveInAndReplaceFileWithDirectory) {
  fs::create_directories(temporary_ / "incoming/sub");
  std::ofstream(temporary_ / "incoming/sub/file");
  std::ofstream(root_ / "item");
  ASSERT_NO_FATAL_FAILURE(start());

  fs::rename(temporary_ / "incoming", root_ / "incoming");
  expect_paths({"/incoming/sub/file"});

  fs::remove(root_ / "item");
  fs::create_directory(root_ / "item");
  std::ofstream(root_ / "item/child") << "new";
  expect_paths({"/item", "/item/child"});
}

TEST_F(MonitorTest, LiteralIgnoreOnBothSidesOfMove) {
  fs::create_directories(root_ / ".meadow/sub");
  std::ofstream(root_ / ".meadow/sub/file") << "existing";
  ASSERT_NO_FATAL_FAILURE(start({"/.meadow", "/*.tmp"}));

  fs::rename(root_ / ".meadow", root_ / ".meadow-old");
  expect_paths({"/.meadow-old/sub/file"});
  fs::rename(root_ / ".meadow-old", root_ / ".meadow");
  expect_paths({"/.meadow-old/sub/file"});

  fs::create_directories(root_ / "nested/.meadow");
  std::ofstream(root_ / "nested/.meadow/file");
  std::ofstream(root_ / "*.tmp");
  std::ofstream(root_ / "other.tmp");
  expect_paths({"/nested/.meadow/file", "/other.tmp"});
  expect_quiet();
}

} // namespace
