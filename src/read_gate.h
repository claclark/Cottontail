#ifndef COTTONTAIL_SRC_READ_GATE_H_
#define COTTONTAIL_SRC_READ_GATE_H_

#include <cerrno>
#include <condition_variable>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>

#include <fcntl.h>
#include <unistd.h>

#include "src/core.h"

namespace cottontail {

class ReadGate final {
public:
  static constexpr size_t DEFAULT_READERS = 2;

  static std::shared_ptr<ReadGate> make(const std::string &filename,
                                        std::string *error = nullptr,
                                        size_t readers = DEFAULT_READERS) {
    if (readers == 0) {
      safe_error(error) = "ReadGate needs at least one reader";
      return nullptr;
    }
    std::shared_ptr<ReadGate> gate =
        std::shared_ptr<ReadGate>(new ReadGate(readers));
    gate->filename_ = filename;
    gate->fd_ = open(filename.c_str(), O_RDONLY);
    if (gate->fd_ < 0) {
      safe_error(error) =
          "ReadGate can't open: " + filename + ": " + std::strerror(errno);
      return nullptr;
    }
    return gate;
  }

  ~ReadGate() {
    if (fd_ >= 0)
      close(fd_);
  }

  ReadGate(const ReadGate &) = delete;
  ReadGate &operator=(const ReadGate &) = delete;
  ReadGate(ReadGate &&) = delete;
  ReadGate &operator=(ReadGate &&) = delete;

  std::unique_ptr<char[]> read(addr offset, addr length,
                               std::string *error = nullptr) {
    if (offset < 0 || length < 0) {
      safe_error(error) = "ReadGate got a negative read range";
      return nullptr;
    }
    std::unique_ptr<char[]> buffer(new char[length == 0 ? 1 : length]);
    Permit permit(this);
    addr done = 0;
    while (done < length) {
      ssize_t n = pread(fd_, buffer.get() + done, length - done, offset + done);
      if (n < 0) {
        if (errno == EINTR)
          continue;
        safe_error(error) =
            "ReadGate can't read: " + filename_ + ": " + std::strerror(errno);
        return nullptr;
      }
      if (n == 0) {
        safe_error(error) = "ReadGate got a short read: " + filename_;
        return nullptr;
      }
      done += n;
    }
    return buffer;
  }

private:
  explicit ReadGate(size_t readers) : available_(readers){};

  void acquire() {
    std::unique_lock<std::mutex> lock(mutex_);
    condition_.wait(lock, [&] { return available_ > 0; });
    --available_;
  }

  void release() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      ++available_;
    }
    condition_.notify_one();
  }

  class Permit final {
  public:
    explicit Permit(ReadGate *gate) : gate_(gate) { gate_->acquire(); }
    ~Permit() { gate_->release(); }
    Permit(const Permit &) = delete;
    Permit &operator=(const Permit &) = delete;
    Permit(Permit &&) = delete;
    Permit &operator=(Permit &&) = delete;

  private:
    ReadGate *gate_;
  };

  std::string filename_;
  int fd_ = -1;
  std::mutex mutex_;
  std::condition_variable condition_;
  size_t available_;
};

} // namespace cottontail

#endif // COTTONTAIL_SRC_READ_GATE_H_
