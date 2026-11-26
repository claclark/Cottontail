#include "src/monitor.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <exception>
#include <filesystem>
#include <mutex>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "efsw/efsw.hpp"
#include "src/core.h"

namespace cottontail {
namespace {

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
using std::chrono::milliseconds;

constexpr milliseconds initial_delay(1000);
constexpr milliseconds maximum_delay(256000);
constexpr milliseconds history_window(300000);

} // namespace

class Monitor::Impl final : public efsw::FileWatchListener {
public:
  explicit Impl(const std::string &directory) : directory_(directory) {}
  ~Impl() override { stop(); }

  bool start(std::string *error) {
    watcher_ = std::make_unique<efsw::FileWatcher>();
    watcher_->followSymlinks(false);
    if (watcher_->addWatch(directory_.string(), this, true) <= 0) {
      safe_error(error) = "Cannot watch directory " + directory_.string() +
                          ": " + efsw::Errors::Log::getLastErrorLog();
      return false;
    }
    worker_ = std::thread(&Impl::run, this);
    watcher_->watch();
    return true;
  }

  std::string next() {
    std::unique_lock<std::mutex> lock(mutex_);
    ready_.wait(lock, [this] { return stopped_ || !queue_.empty(); });
    if (queue_.empty())
      return "";
    std::string path = std::move(queue_.front());
    queue_.pop_front();
    queued_.erase(path);
    return path;
  }

  void stop() {
    std::call_once(stop_once_, [this] {
      {
        std::lock_guard<std::mutex> lock(mutex_);
        stopped_ = true;
      }
      changed_.notify_one();
      ready_.notify_all();
      watcher_.reset();
      if (worker_.joinable())
        worker_.join();
    });
  }

private:
  struct Pending {
    milliseconds delay;
    Clock::time_point deadline;
    std::uint64_t order;
  };

  struct Bounce {
    milliseconds delay;
    Clock::time_point enqueued;
  };

  void handleFileAction(efsw::WatchID, const std::string &directory,
                        const std::string &filename, efsw::Action action,
                        const std::string &old_filename) override {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto now = Clock::now();
    const auto deadline = schedule(fs::path(directory) / filename, now);
    if (action == efsw::Actions::Moved && !old_filename.empty())
      schedule(fs::path(directory) / old_filename, now, deadline);
  }

  void handleMissedFileActions(efsw::WatchID,
                               const std::string &directory) override {
    std::lock_guard<std::mutex> lock(mutex_);
    schedule(directory, Clock::now());
  }

  // Called with mutex_ held. Return the deadline for the scheduled path.
  Clock::time_point schedule(
      fs::path path, Clock::time_point now,
      Clock::time_point earliest = Clock::time_point::min()) {
    path = path.lexically_normal();
    if (path.has_relative_path() && !path.has_filename())
      path = path.parent_path();
    const std::string filename = path.string();
    if (stopped_ || queued_.contains(filename))
      return now;

    const auto relative = path.lexically_relative(directory_);
    if (relative.empty() || *relative.begin() == ".." ||
        std::any_of(relative.begin(), relative.end(), [](const fs::path &part) {
          return part != "." && part.native().starts_with('.');
        }))
      return now;

    auto pending = pending_.find(filename);
    if (pending == pending_.end()) {
      std::erase_if(bounce_, [now](const auto &entry) {
        return now - entry.second.enqueued >= history_window;
      });
      auto last = bounce_.find(filename);
      auto delay = last == bounce_.end()
                       ? initial_delay
                       : std::min(last->second.delay * 2, maximum_delay);
      pending = pending_.emplace(filename, Pending{delay, now, 0}).first;
    }
    pending->second.deadline =
        std::max({pending->second.deadline, now + pending->second.delay, earliest});
    pending->second.order = order_++;
    if (pending->second.deadline < wake_at_)
      changed_.notify_one();
    return pending->second.deadline;
  }

  bool syncable(const fs::path &path) const {
    const auto relative = path.lexically_relative(directory_);
    // Check parents in order so an event cannot redirect us through a symlink.
    std::error_code error;
    auto parent = directory_;
    for (const auto &part : relative) {
      auto status = fs::symlink_status(parent, error);
      if (status.type() == fs::file_type::not_found)
        return true;
      if (!fs::is_directory(status))
        return false;
      parent /= part;
    }
    auto status = fs::symlink_status(path, error);
    return fs::is_regular_file(status) || fs::is_directory(status) ||
           status.type() == fs::file_type::not_found;
  }

  void run() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (!stopped_) {
      const auto now = Clock::now();
      wake_at_ = Clock::time_point::max();
      std::vector<decltype(pending_)::iterator> due;
      for (auto item = pending_.begin(); item != pending_.end(); ++item) {
        if (item->second.deadline <= now)
          due.push_back(item);
        else
          wake_at_ = std::min(wake_at_, item->second.deadline);
      }
      std::sort(due.begin(), due.end(), [](auto left, auto right) {
        const auto &a = left->second;
        const auto &b = right->second;
        return std::tie(a.deadline, a.order) < std::tie(b.deadline, b.order);
      });
      for (auto item : due) {
        const auto &[path, pending] = *item;
        if (!queued_.contains(path) && syncable(path)) {
          queued_.insert(path);
          queue_.push_back(path);
          bounce_.insert_or_assign(path, Bounce{pending.delay, Clock::now()});
          ready_.notify_one();
        }
        pending_.erase(item);
      }
      if (wake_at_ == Clock::time_point::max())
        changed_.wait(lock);
      else
        changed_.wait_until(lock, wake_at_);
    }
  }

  const fs::path directory_;
  std::mutex mutex_;
  std::condition_variable changed_, ready_;
  std::deque<std::string> queue_;
  std::unordered_set<std::string> queued_;
  std::unordered_map<std::string, Pending> pending_;
  std::unordered_map<std::string, Bounce> bounce_;
  Clock::time_point wake_at_ = Clock::time_point::max();
  std::uint64_t order_ = 0;
  bool stopped_ = false;
  std::once_flag stop_once_;
  std::thread worker_;
  std::unique_ptr<efsw::FileWatcher> watcher_;
};

std::shared_ptr<Monitor> Monitor::make(const std::string &path,
                                       std::string *error) {
  if (path.empty()) {
    safe_error(error) = "Missing directory to monitor";
    return nullptr;
  }

  fs::path directory(path);
  while (directory.has_relative_path() &&
         (!directory.has_filename() || directory.filename() == "."))
    directory = directory.parent_path();
  if (directory.empty())
    directory = ".";

  std::error_code failure;
  const auto status = fs::symlink_status(directory, failure);
  if (failure || !fs::is_directory(status)) {
    safe_error(error) = "Cannot monitor directory " + path + ": " +
                        (failure ? failure.message() : "expected a directory");
    return nullptr;
  }
  directory = fs::canonical(directory, failure);
  if (failure) {
    safe_error(error) = "Cannot resolve directory " + path + ": " +
                        failure.message();
    return nullptr;
  }

  try {
    auto monitor = std::shared_ptr<Monitor>(new Monitor(directory.string()));
    if (!monitor->impl_->start(error))
      return nullptr;
    return monitor;
  } catch (const std::exception &exception) {
    safe_error(error) = "Cannot start monitor for " + path + ": " +
                        exception.what();
    return nullptr;
  }
}

Monitor::Monitor(const std::string &directory)
    : impl_(std::make_unique<Impl>(directory)) {}

Monitor::~Monitor() = default;

std::string Monitor::next() { return impl_->next(); }

void Monitor::stop() { impl_->stop(); }

} // namespace cottontail
