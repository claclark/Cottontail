#ifndef COTTONTAIL_SRC_CACHE_GATE_H_
#define COTTONTAIL_SRC_CACHE_GATE_H_

#include <atomic>
#include <version>

#include "src/hints.h"

#if !defined(__cpp_lib_atomic_wait) || __cpp_lib_atomic_wait < 201907L
#include <condition_variable>
#include <mutex>
#endif

namespace cottontail {

#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L

class CacheGate final {
public:
  CacheGate() : CacheGate(true){};
  explicit CacheGate(bool open) : open_(open){};

  CacheGate(const CacheGate &) = delete;
  CacheGate &operator=(const CacheGate &) = delete;
  CacheGate(CacheGate &&) = delete;
  CacheGate &operator=(CacheGate &&) = delete;

  bool is_open() const { return open_.load(std::memory_order_acquire); }

  void wait() const {
    if (LIKELY(is_open()))
      return;
    do {
      open_.wait(false, std::memory_order_acquire);
    } while (!is_open());
  }

  void open() {
    bool closed = false;
    if (open_.compare_exchange_strong(closed, true, std::memory_order_release,
                                      std::memory_order_relaxed))
      open_.notify_all();
  }

private:
  std::atomic<bool> open_;
};

#else

class CacheGate final {
public:
  CacheGate() : CacheGate(true){};
  explicit CacheGate(bool open) : open_(open){};

  CacheGate(const CacheGate &) = delete;
  CacheGate &operator=(const CacheGate &) = delete;
  CacheGate(CacheGate &&) = delete;
  CacheGate &operator=(CacheGate &&) = delete;

  bool is_open() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return open_;
  }

  void wait() const {
    if (LIKELY(is_open()))
      return;
    std::unique_lock<std::mutex> lock(mutex_);
    condition_.wait(lock, [&] { return open_; });
  }

  void open() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (open_)
        return;
      open_ = true;
    }
    condition_.notify_all();
  }

private:
  mutable std::mutex mutex_;
  mutable std::condition_variable condition_;
  bool open_;
};

#endif

} // namespace cottontail

#endif // COTTONTAIL_SRC_CACHE_GATE_H_
