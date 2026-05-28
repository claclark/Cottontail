#include "src/async.h"

#include <algorithm>
#include <cstring>

namespace cottontail {

std::unique_ptr<AsyncReader>
AsyncReader::make(const std::string &filename, addr offset, addr length,
                  size_t buffer_size, std::string *error) {
  if (offset < 0 || length < 0) {
    safe_error(error) = "AsyncReader got a negative range";
    return nullptr;
  }
  if (buffer_size == 0) {
    safe_error(error) = "AsyncReader needs a positive buffer size";
    return nullptr;
  }
  std::unique_ptr<AsyncReader> reader(new AsyncReader());
  reader->filename_ = filename;
  reader->offset_ = offset;
  reader->length_ = length;
  reader->buffer_size_ = buffer_size;
  reader->buffers_[0].data.resize(buffer_size);
  reader->buffers_[1].data.resize(buffer_size);
  reader->in_.open(filename, std::ios::binary | std::ios::in);
  if (reader->in_.fail()) {
    safe_error(error) = "AsyncReader can't open: " + filename;
    return nullptr;
  }
  reader->worker_ = std::thread(&AsyncReader::worker, reader.get());
  return reader;
}

AsyncReader::~AsyncReader() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stop_ = true;
  }
  can_fill_.notify_all();
  can_read_.notify_all();
  if (worker_.joinable())
    worker_.join();
}

bool AsyncReader::read(char *data, addr length, std::string *error) {
  if (length < 0) {
    safe_error(error) = "AsyncReader got a negative read length";
    return false;
  }
  return consume(data, length, error);
}

bool AsyncReader::read(std::string *data, addr length, std::string *error) {
  if (data == nullptr) {
    safe_error(error) = "AsyncReader got a null string";
    return false;
  }
  if (length < 0) {
    safe_error(error) = "AsyncReader got a negative read length";
    return false;
  }
  data->assign(length, '\0');
  if (length == 0)
    return true;
  return consume(&(*data)[0], length, error);
}

bool AsyncReader::skip(addr length, std::string *error) {
  if (length < 0) {
    safe_error(error) = "AsyncReader got a negative skip length";
    return false;
  }
  std::vector<char> scratch(std::min<addr>(length, buffer_size_));
  addr left = length;
  while (left > 0) {
    addr chunk = std::min<addr>(left, scratch.size());
    if (!consume(scratch.data(), chunk, error))
      return false;
    left -= chunk;
  }
  return true;
}

bool AsyncReader::consume(char *data, addr length, std::string *error) {
  addr copied = 0;
  while (copied < length) {
    std::unique_lock<std::mutex> lock(mutex_);
    can_read_.wait(lock, [&] {
      return stop_ || buffers_[read_buffer_].full || done_ || error_ != "";
    });
    if (error_ != "") {
      safe_error(error) = error_;
      return false;
    }
    if (!buffers_[read_buffer_].full) {
      safe_error(error) = "AsyncReader got a short read: " + filename_;
      return false;
    }
    Buffer &buffer = buffers_[read_buffer_];
    addr available = buffer.size - buffer.used;
    addr chunk = std::min(length - copied, available);
    memcpy(data + copied, buffer.data.data() + buffer.used, chunk);
    buffer.used += chunk;
    copied += chunk;
    consumed_ += chunk;
    if (buffer.used == buffer.size) {
      buffer.full = false;
      buffer.size = 0;
      buffer.used = 0;
      read_buffer_ = 1 - read_buffer_;
      can_fill_.notify_one();
    }
  }
  return true;
}

void AsyncReader::worker() {
  if (length_ > 0) {
    in_.clear();
    in_.seekg(offset_, in_.beg);
    if (in_.fail()) {
      std::lock_guard<std::mutex> lock(mutex_);
      error_ = "AsyncReader can't seek: " + filename_;
      can_read_.notify_all();
      return;
    }
  }

  while (true) {
    Buffer *buffer;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      can_fill_.wait(lock, [&] {
        return stop_ || !buffers_[fill_buffer_].full;
      });
      if (stop_)
        return;
      if (filled_ >= length_) {
        done_ = true;
        can_read_.notify_all();
        return;
      }
      buffer = &buffers_[fill_buffer_];
      buffer->offset = filled_;
    }

    addr want = std::min<addr>(buffer_size_, length_ - buffer->offset);
    in_.read(buffer->data.data(), want);
    addr got = in_.gcount();

    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (in_.fail() || got != want) {
        error_ = "AsyncReader can't read: " + filename_;
        can_read_.notify_all();
        return;
      }
      buffer->size = got;
      buffer->used = 0;
      buffer->full = true;
      filled_ += got;
      fill_buffer_ = 1 - fill_buffer_;
    }
    can_read_.notify_all();
  }
}

std::unique_ptr<AsyncWriter>
AsyncWriter::make(const std::string &filename, size_t buffer_size,
                  std::string *error) {
  if (buffer_size == 0) {
    safe_error(error) = "AsyncWriter needs a positive buffer size";
    return nullptr;
  }
  std::unique_ptr<AsyncWriter> writer(new AsyncWriter());
  writer->filename_ = filename;
  writer->buffer_size_ = buffer_size;
  writer->buffers_[0].data.resize(buffer_size);
  writer->buffers_[1].data.resize(buffer_size);
  writer->out_.open(filename, std::ios::binary | std::ios::out);
  if (writer->out_.fail()) {
    safe_error(error) = "AsyncWriter can't open: " + filename;
    return nullptr;
  }
  writer->worker_ = std::thread(&AsyncWriter::worker, writer.get());
  return writer;
}

AsyncWriter::~AsyncWriter() {
  std::string ignored;
  close(&ignored);
}

bool AsyncWriter::write(const char *data, addr length, std::string *error) {
  if (length < 0) {
    safe_error(error) = "AsyncWriter got a negative write length";
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (closed_) {
      safe_error(error) = "AsyncWriter is closed: " + filename_;
      return false;
    }
  }
  addr copied = 0;
  while (copied < length) {
    if (!flush_buffer(error))
      return false;
    Buffer &buffer = buffers_[write_buffer_];
    addr chunk = std::min<addr>(length - copied, buffer_size_ - buffer.size);
    memcpy(buffer.data.data() + buffer.size, data + copied, chunk);
    buffer.size += chunk;
    copied += chunk;
  }
  return true;
}

bool AsyncWriter::write(const std::string &data, std::string *error) {
  return write(data.data(), data.size(), error);
}

bool AsyncWriter::close(std::string *error) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (closed_) {
      if (error_ != "") {
        safe_error(error) = error_;
        return false;
      }
      return true;
    }
  }
  bool ok = flush_buffer(error);
  {
    std::unique_lock<std::mutex> lock(mutex_);
    if (error_ == "") {
      Buffer &buffer = buffers_[write_buffer_];
      if (buffer.size > 0) {
        can_fill_.wait(lock, [&] {
          return error_ != "" || !buffers_[1 - write_buffer_].full;
        });
        if (error_ == "") {
          buffer.full = true;
          write_buffer_ = 1 - write_buffer_;
          can_flush_.notify_one();
        }
      }
    }
  }
  {
    std::lock_guard<std::mutex> lock(mutex_);
    closed_ = true;
    stop_ = true;
  }
  can_flush_.notify_all();
  can_fill_.notify_all();
  if (worker_.joinable())
    worker_.join();
  out_.close();
  if (out_.fail()) {
    safe_error(error) = "AsyncWriter can't close: " + filename_;
    return false;
  }
  if (error_ != "") {
    safe_error(error) = error_;
    return false;
  }
  return ok;
}

bool AsyncWriter::flush_buffer(std::string *error) {
  std::unique_lock<std::mutex> lock(mutex_);
  if (error_ != "") {
    safe_error(error) = error_;
    return false;
  }
  Buffer &buffer = buffers_[write_buffer_];
  if (buffer.size < (addr)buffer_size_)
    return true;
  can_fill_.wait(lock, [&] {
    return error_ != "" || !buffers_[1 - write_buffer_].full;
  });
  if (error_ != "") {
    safe_error(error) = error_;
    return false;
  }
  buffer.full = true;
  write_buffer_ = 1 - write_buffer_;
  can_flush_.notify_one();
  return true;
}

void AsyncWriter::worker() {
  while (true) {
    Buffer *buffer = nullptr;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      can_flush_.wait(lock, [&] {
        return stop_ || buffers_[flush_buffer_].full;
      });
      if (!buffers_[flush_buffer_].full) {
        if (stop_)
          return;
        continue;
      }
      buffer = &buffers_[flush_buffer_];
    }

    out_.write(buffer->data.data(), buffer->size);

    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (out_.fail())
        error_ = "AsyncWriter can't write: " + filename_;
      buffer->size = 0;
      buffer->full = false;
      flush_buffer_ = 1 - flush_buffer_;
    }
    can_fill_.notify_one();
  }
}

} // namespace cottontail
