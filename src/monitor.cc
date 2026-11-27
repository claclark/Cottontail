#include "src/monitor.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <exception>
#include <filesystem>
#include <mutex>
#include <set>
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
  Impl(const std::string &directory, const std::vector<std::string> &ignore)
      : directory_(directory), ignore_(ignore.begin(), ignore.end()) {}
  ~Impl() override { stop(); }

  bool start(std::string *error) {
    watcher_ = std::make_unique<efsw::FileWatcher>();
    watcher_->followSymlinks(false);
    // Catch files created between discovering a directory and installing its
    // inotify watch. The debounce table coalesces duplicate notifications.
    if (watcher_->addWatch(directory_.string(), this, true,
                           {{efsw::Options::LinuxProduceSyntheticEvents, 1}}) <=
        0) {
      safe_error(error) = "Cannot watch directory " + directory_.string() +
                          ": " + efsw::Errors::Log::getLastErrorLog();
      return false;
    }
    watcher_->watch();
    {
      // Callbacks received during the walk still go through normal delivery.
      std::lock_guard<std::mutex> lock(mutex_);
      std::error_code failure;
      if (!ignore_.contains("/"))
        scan(directory_, &files_, &failure);
      if (failure) {
        safe_error(error) =
            "Cannot scan " + directory_.string() + ": " + failure.message();
        return false;
      }
    }
    worker_ = std::thread(&Impl::run, this);
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
    const auto deadline = reconcile(fs::path(directory) / filename, now,
                                    action != efsw::Actions::Modified);
    if (action == efsw::Actions::Moved && !old_filename.empty())
      reconcile(fs::path(directory) / old_filename, now, false, deadline);
  }

  void handleMissedFileActions(efsw::WatchID,
                               const std::string &directory) override {
    std::lock_guard<std::mutex> lock(mutex_);
    // Even a file still present at the same path may have been modified.
    const auto now = Clock::now();
    reconcile(directory, now, true);
  }

  // Collect the visible regular files. A disappearing entry is normal during
  // a live walk. Skip symlinks, including symlinks to directories.
  void scan(const fs::path &directory, std::set<std::string> *files,
            std::error_code *error) const {
    fs::directory_iterator it(directory, *error), end;
    for (; !*error && it != end; it.increment(*error)) {
      const fs::path &path = it->path();
      const auto name = "/" + path.lexically_relative(directory_).generic_string();
      if (ignore_.contains(name))
        continue;
      const auto status = it->symlink_status(*error);
      if (status.type() == fs::file_type::not_found) {
        error->clear();
        continue;
      }
      if (*error)
        break;
      if (fs::is_regular_file(status))
        files->insert(name);
      else if (fs::is_directory(status)) {
        scan(path, files, error);
        if (*error)
          break;
      }
    }
    if (*error == std::errc::no_such_file_or_directory ||
        *error == std::errc::not_a_directory)
      error->clear();
  }

  // Called with mutex_ held. Return the latest deadline so a move can
  // schedule the former names after the new ones.
  Clock::time_point
  reconcile(fs::path path, Clock::time_point now, bool report_existing,
            Clock::time_point earliest = Clock::time_point::min()) {
    path = path.lexically_normal();
    if (path.has_relative_path() && !path.has_filename())
      path = path.parent_path();
    const auto relative = path.lexically_relative(directory_);
    if (stopped_ || relative.empty() || *relative.begin() == ".." ||
        ignore_.contains("/"))
      return now;

    const bool root = relative == ".";
    std::string name;
    if (root) {
      name = "/";
    } else {
      for (const auto &part : relative) {
        name += "/" + part.generic_string();
        if (ignore_.contains(name))
          return now;
      }
    }

    // Check ancestors before looking at the event path. A directory may have
    // been replaced with a symlink since the event was generated.
    std::error_code error;
    auto parent = directory_;
    auto status = fs::symlink_status(parent, error);
    for (const auto &part : relative) {
      if (!fs::is_directory(status))
        break;
      parent /= part;
      status = fs::symlink_status(parent, error);
    }
    if (error && status.type() != fs::file_type::not_found)
      return now;

    auto latest = now;
    auto report = [&](const std::string &file) {
      latest = std::max(latest, schedule(file, now, earliest));
    };

    if (!root && parent == path && fs::is_regular_file(status)) {
      files_.insert(name);
      report(name);
    } else if (files_.erase(name)) {
      report(name);
    }

    // Only a directory event needs a walk. A missing path may still have
    // remembered descendants, as when a populated directory is moved away.
    const std::string prefix = root ? "/" : name + "/";
    std::set<std::string> present;
    if (fs::is_directory(status)) {
      scan(path, &present, &error);
      for (const auto &file : present)
        if (files_.insert(file).second || report_existing)
          report(file);
      if (error)
        return latest;
    }
    auto file = files_.lower_bound(prefix);
    while (file != files_.end() && file->starts_with(prefix)) {
      if (!present.contains(*file)) {
        report(*file);
        file = files_.erase(file);
      } else {
        ++file;
      }
    }
    return latest;
  }

  Clock::time_point schedule(const std::string &filename, Clock::time_point now,
                             Clock::time_point earliest) {
    if (queued_.contains(filename))
      return now;
    auto pending = pending_.find(filename);
    if (pending == pending_.end()) {
      // All paths expanded from one callback share the same timestamp.
      if (now != last_purge_) {
        std::erase_if(bounce_, [now](const auto &entry) {
          return now - entry.second.enqueued >= history_window;
        });
        last_purge_ = now;
      }
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
        if (queued_.insert(path).second) {
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
  const std::unordered_set<std::string> ignore_;
  std::mutex mutex_;
  std::condition_variable changed_, ready_;
  std::set<std::string> files_;
  std::deque<std::string> queue_;
  std::unordered_set<std::string> queued_;
  std::unordered_map<std::string, Pending> pending_;
  std::unordered_map<std::string, Bounce> bounce_;
  Clock::time_point last_purge_ = Clock::time_point::min();
  Clock::time_point wake_at_ = Clock::time_point::max();
  std::uint64_t order_ = 0;
  bool stopped_ = false;
  std::once_flag stop_once_;
  std::thread worker_;
  std::unique_ptr<efsw::FileWatcher> watcher_;
};

std::shared_ptr<Monitor> Monitor::make(const std::string &path,
                                       const std::vector<std::string> &ignore,
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
    auto monitor = std::shared_ptr<Monitor>(new Monitor(directory.string(), ignore));
    if (!monitor->impl_->start(error))
      return nullptr;
    return monitor;
  } catch (const std::exception &exception) {
    safe_error(error) = "Cannot start monitor for " + path + ": " +
                        exception.what();
    return nullptr;
  }
}

Monitor::Monitor(const std::string &directory, const std::vector<std::string> &ignore)
    : impl_(std::make_unique<Impl>(directory, ignore)) {}

Monitor::~Monitor() = default;

std::string Monitor::next() { return impl_->next(); }

void Monitor::stop() { impl_->stop(); }

} // namespace cottontail
