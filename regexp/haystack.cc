#include "regexp/haystack.h"

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

namespace cottontail {
namespace regexp {
namespace {

class FullHaystack final : public Haystack {
public:
  explicit FullHaystack(std::string filename)
      : filename_(std::move(filename)), replayable_(true) {
    load();
  }
  explicit FullHaystack(std::shared_ptr<std::istream> input)
      : input_(std::move(input)), replayable_(false) {}

  bool chunk(const char **start, const char **end) final {
    touched_ = true;
    if (start == nullptr || end == nullptr) {
      fail("Haystack::chunk got a null pointer");
      return false;
    }
    if (!error_.empty() || delivered_)
      return false;
    if (!loaded_ && !load())
      return false;
    delivered_ = true;
    if (size_ == 0)
      return false;
    *start = data();
    *end = data() + size_;
    return true;
  }

  std::string translate(addr p, addr q) final {
    const char *start;
    const char *end;
    if (!translate(p, q, &start, &end))
      return "";
    return std::string(start, end);
  }

  bool translate(addr p, addr q, const char **start, const char **end) final {
    if (start == nullptr || end == nullptr) {
      fail("Haystack::translate got a null pointer");
      return false;
    }
    if (!error_.empty())
      return false;
    if (!loaded_) {
      fail("Haystack::translate called before input was read");
      return false;
    }
    if (p < 0 || q < p || static_cast<std::size_t>(q) >= size_) {
      fail("Haystack::translate interval is outside the input");
      return false;
    }
    *start = data() + p;
    *end = data() + q + 1;
    return true;
  }

  void limit(addr x) final {
    if (x > limit_)
      limit_ = x;
  }

  bool reset(std::string *error) final {
    if (!touched_) {
      error_.clear();
      return true;
    }
    if (!replayable_) {
      safe_error(error) = "Cannot reset a consumed one-shot Haystack";
      return false;
    }
    error_.clear();
    delivered_ = false;
    touched_ = false;
    limit_ = -1;
    if (!loaded_) {
      bytes_.clear();
      buffer_.reset();
      size_ = 0;
    }
    return true;
  }

  bool success(std::string *error) final {
    if (error_.empty())
      return true;
    safe_error(error) = error_;
    return false;
  }

private:
  const char *data() const { return buffer_ ? buffer_.get() : bytes_.data(); }

  bool read(std::istream &input) {
    char buffer[64 * 1024];
    while (input.read(buffer, sizeof(buffer)))
      bytes_.append(buffer, sizeof(buffer));
    bytes_.append(buffer, input.gcount());
    size_ = bytes_.size();
    return input.eof() && !input.bad();
  }

  bool load() {
    bytes_.clear();
    buffer_.reset();
    size_ = 0;
    if (replayable_) {
      std::ifstream input(filename_, std::ios::binary);
      if (!input) {
        fail("Cannot open input: " + filename_);
        return false;
      }
      input.seekg(0, std::ios::end);
      std::streamoff length = input.tellg();
      input.clear();
      input.seekg(0, std::ios::beg);
      if (length >= 0 && input) {
        size_ = static_cast<std::size_t>(length);
        buffer_.reset(new char[size_]);
        input.read(buffer_.get(), size_);
        size_ = static_cast<std::size_t>(input.gcount());
        if (input.bad() || (input.fail() && !input.eof())) {
          fail("Cannot read input: " + filename_);
          return false;
        }
      } else {
        // Pipes and other unsized inputs still need the growing buffer.
        input.clear();
        if (!read(input)) {
          fail("Cannot read input: " + filename_);
          return false;
        }
      }
    } else {
      if (!read(*input_)) {
        fail("Cannot read input stream");
        return false;
      }
    }
    loaded_ = true;
    return true;
  }

  void fail(const std::string &message) {
    if (error_.empty())
      error_ = message;
  }

  std::string filename_;
  std::shared_ptr<std::istream> input_;
  std::string bytes_;
  std::unique_ptr<char[]> buffer_;
  std::size_t size_ = 0;
  std::string error_;
  addr limit_ = -1;
  bool replayable_;
  bool loaded_ = false;
  bool delivered_ = false;
  bool touched_ = false;
};

class BufferHaystack final : public Haystack {
public:
  BufferHaystack(std::shared_ptr<const char> buffer, std::size_t size)
      : buffer_(std::move(buffer)), size_(size) {}

  bool chunk(const char **start, const char **end) final {
    if (start == nullptr || end == nullptr) {
      fail("Haystack::chunk got a null pointer");
      return false;
    }
    if (!error_.empty() || delivered_)
      return false;
    delivered_ = true;
    if (size_ == 0)
      return false;
    *start = buffer_.get();
    *end = buffer_.get() + size_;
    return true;
  }

  std::string translate(addr p, addr q) final {
    const char *start;
    const char *end;
    if (!translate(p, q, &start, &end))
      return "";
    return std::string(start, end);
  }

  bool translate(addr p, addr q, const char **start, const char **end) final {
    if (start == nullptr || end == nullptr) {
      fail("Haystack::translate got a null pointer");
      return false;
    }
    if (!error_.empty())
      return false;
    if (p < 0 || q < p || static_cast<std::size_t>(q) >= size_) {
      fail("Translation outside buffer Haystack");
      return false;
    }
    *start = buffer_.get() + p;
    *end = buffer_.get() + q + 1;
    return true;
  }

  void limit(addr x) final { (void)x; }

  bool reset(std::string *error) final {
    (void)error;
    error_.clear();
    delivered_ = false;
    return true;
  }

  bool success(std::string *error) final {
    if (error_.empty())
      return true;
    safe_error(error) = error_;
    return false;
  }

private:
  void fail(const std::string &message) {
    if (error_.empty())
      error_ = message;
  }
  std::shared_ptr<const char> buffer_;
  std::size_t size_;
  bool delivered_ = false;
  std::string error_;
};

} // namespace

std::shared_ptr<Haystack>
Haystack::make(std::shared_ptr<const char> buffer, std::size_t size,
                std::string *error) {
  if (buffer == nullptr || size > static_cast<std::size_t>(maxfinity)) {
    safe_error(error) = "Haystack needs a valid byte buffer";
    return nullptr;
  }
  return std::make_shared<BufferHaystack>(std::move(buffer), size);
}

std::shared_ptr<Haystack>
Haystack::make(std::shared_ptr<std::istream> input, std::string *error) {
  if (input == nullptr || !*input) {
    safe_error(error) = "Haystack needs a readable input stream";
    return nullptr;
  }
  return std::make_shared<FullHaystack>(std::move(input));
}

std::shared_ptr<Haystack> Haystack::make(const std::string &filename,
                                         std::string *error) {
  std::shared_ptr<Haystack> haystack = std::make_shared<FullHaystack>(filename);
  if (!haystack->success(error))
    return nullptr;
  return haystack;
}

std::shared_ptr<Haystack> Haystack::make_stdin(std::string *error) {
  return make(std::shared_ptr<std::istream>(&std::cin, [](std::istream *) {}),
              error);
}

} // namespace regexp
} // namespace cottontail
