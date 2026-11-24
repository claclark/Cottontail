#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "efsw/efsw.hpp"
#include "gtest/gtest.h"

namespace {

namespace fs = std::filesystem;

const char *action_name(efsw::Action action) {
  switch (action) {
  case efsw::Actions::Add:
    return "Add";
  case efsw::Actions::Delete:
    return "Delete";
  case efsw::Actions::Modified:
    return "Modified";
  case efsw::Actions::Moved:
    return "Moved";
  }
  return "Unknown";
}

class Listener : public efsw::FileWatchListener {
public:
  bool wait(const fs::path &file, efsw::Action action) {
    std::unique_lock<std::mutex> lock(mutex_);
    return changed_.wait_for(lock, std::chrono::seconds(5), [&] {
      auto entry = actions_.find(file);
      return entry != actions_.end() && entry->second.contains(action);
    });
  }

private:
  void handleFileAction(efsw::WatchID, const std::string &directory,
                        const std::string &filename, efsw::Action action,
                        const std::string &) override {
    auto file = (fs::path(directory) / filename).lexically_normal();
    {
      std::lock_guard<std::mutex> lock(mutex_);
      std::cout << action_name(action) << ' ' << file << '\n';
      actions_[file].insert(action);
    }
    changed_.notify_all();
  }

  std::mutex mutex_;
  std::condition_variable changed_;
  std::map<fs::path, std::set<efsw::Action>> actions_;
};

class FileWatcherTest : public ::testing::Test {
protected:
  void SetUp() override {
    const char *tmp = std::getenv("TEST_TMPDIR");
    const fs::path base = tmp == nullptr ? fs::temp_directory_path() : tmp;
    std::string pattern = (base / "filewatcher-XXXXXX").string();
    ASSERT_NE(mkdtemp(pattern.data()), nullptr);
    root_ = pattern;
    root_ = fs::canonical(root_);
  }

  void TearDown() override {
    if (!root_.empty()) {
      std::error_code error;
      fs::remove_all(root_, error);
      EXPECT_FALSE(error) << error.message();
    }
  }

  fs::path root_;
};

TEST_F(FileWatcherTest, ConcurrentAddAndDelete) {
  std::vector<fs::path> directories;
  for (int i = 0; i < 3; ++i) {
    auto directory = root_ / ("worker-" + std::to_string(i));
    ASSERT_TRUE(fs::create_directory(directory));
    directories.push_back(directory);
  }

  Listener listener;
  efsw::FileWatcher watcher;
  ASSERT_GT(watcher.addWatch(root_.string(), &listener, true), 0)
      << efsw::Errors::Log::getLastErrorLog();
  watcher.watch();

  std::vector<std::jthread> workers;
  for (const auto &directory : directories) {
    workers.emplace_back([&listener, directory] {
      for (int i = 0; i < 3; ++i) {
        auto file = directory / ("junk-" + std::to_string(i) + ".txt");
        std::ofstream output(file);
        output << "junk\n";
        output.close();
        ASSERT_TRUE(output) << file;
        ASSERT_TRUE(listener.wait(file, efsw::Actions::Add))
            << "Missing Add for " << file;

        std::error_code error;
        ASSERT_TRUE(fs::remove(file, error)) << file << " " << error.message();
        ASSERT_TRUE(listener.wait(file, efsw::Actions::Delete))
            << "Missing Delete for " << file;
      }
    });
  }
}

} // namespace
