#ifndef COTTONTAIL_SRC_ASYNC_H_
#define COTTONTAIL_SRC_ASYNC_H_

#include <condition_variable>
#include <cstddef>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "src/core.h"

namespace cottontail {

class AsyncReader final {
public:
  static std::unique_ptr<AsyncReader>
  make(const std::string &filename, addr offset, addr length,
       size_t buffer_size, std::string *error = nullptr);

  ~AsyncReader();
  AsyncReader(const AsyncReader &) = delete;
  AsyncReader &operator=(const AsyncReader &) = delete;
  AsyncReader(AsyncReader &&) = delete;
  AsyncReader &operator=(AsyncReader &&) = delete;

  bool read(char *data, addr length, std::string *error = nullptr);
  bool read(std::string *data, addr length, std::string *error = nullptr);
  bool skip(addr length, std::string *error = nullptr);

private:
  AsyncReader() = default;

  struct Buffer {
    std::vector<char> data;
    addr offset = 0;
    addr size = 0;
    addr used = 0;
    bool full = false;
  };

  void worker();
  bool consume(char *data, addr length, std::string *error);

  std::string filename_;
  std::ifstream in_;
  addr offset_ = 0;
  addr length_ = 0;
  addr filled_ = 0;
  addr consumed_ = 0;
  size_t buffer_size_ = 0;
  Buffer buffers_[2];
  size_t read_buffer_ = 0;
  size_t fill_buffer_ = 0;
  bool done_ = false;
  bool stop_ = false;
  std::string error_;
  std::mutex mutex_;
  std::condition_variable can_fill_;
  std::condition_variable can_read_;
  std::thread worker_;
};

class AsyncWriter final {
public:
  static std::unique_ptr<AsyncWriter>
  make(const std::string &filename, size_t buffer_size,
       std::string *error = nullptr);

  ~AsyncWriter();
  AsyncWriter(const AsyncWriter &) = delete;
  AsyncWriter &operator=(const AsyncWriter &) = delete;
  AsyncWriter(AsyncWriter &&) = delete;
  AsyncWriter &operator=(AsyncWriter &&) = delete;

  bool write(const char *data, addr length, std::string *error = nullptr);
  bool write(const std::string &data, std::string *error = nullptr);
  bool close(std::string *error = nullptr);

private:
  AsyncWriter() = default;

  struct Buffer {
    std::vector<char> data;
    addr size = 0;
    bool full = false;
  };

  void worker();
  bool flush_buffer(std::string *error);

  std::string filename_;
  std::ofstream out_;
  size_t buffer_size_ = 0;
  Buffer buffers_[2];
  size_t write_buffer_ = 0;
  size_t flush_buffer_ = 0;
  bool closed_ = false;
  bool stop_ = false;
  std::string error_;
  std::mutex mutex_;
  std::condition_variable can_fill_;
  std::condition_variable can_flush_;
  std::thread worker_;
};

} // namespace cottontail

#endif // COTTONTAIL_SRC_ASYNC_H_
