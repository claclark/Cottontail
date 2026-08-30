#include "regexp/haystack.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

namespace cottontail {
namespace regexp {
namespace {

class FullHaystack final : public Haystack {
public:
  explicit FullHaystack(std::string filename)
      : filename_(std::move(filename)), replayable_(true) {}
  FullHaystack() : replayable_(false) {}

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
    if (bytes_.empty())
      return false;
    *start = bytes_.data();
    *end = bytes_.data() + bytes_.size();
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
    if (p < 0 || q < p || static_cast<std::size_t>(q) >= bytes_.size()) {
      fail("Haystack::translate interval is outside the input");
      return false;
    }
    *start = bytes_.data() + p;
    *end = bytes_.data() + q + 1;
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
    if (!loaded_)
      bytes_.clear();
    return true;
  }

  bool success(std::string *error) final {
    if (error_.empty())
      return true;
    safe_error(error) = error_;
    return false;
  }

private:
  bool load() {
    bytes_.clear();
    if (replayable_) {
      std::ifstream input(filename_, std::ios::binary);
      if (!input) {
        fail("Cannot open input: " + filename_);
        return false;
      }
      bytes_.assign(std::istreambuf_iterator<char>(input),
                    std::istreambuf_iterator<char>());
      if (input.bad()) {
        fail("Cannot read input: " + filename_);
        return false;
      }
    } else {
      bytes_.assign(std::istreambuf_iterator<char>(std::cin),
                    std::istreambuf_iterator<char>());
      if (std::cin.bad()) {
        fail("Cannot read standard input");
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
  std::string bytes_;
  std::string error_;
  addr limit_ = -1;
  bool replayable_;
  bool loaded_ = false;
  bool delivered_ = false;
  bool touched_ = false;
};

} // namespace

std::shared_ptr<Haystack> Haystack::make(const std::string &filename,
                                         std::string *error) {
  std::ifstream input(filename, std::ios::binary);
  if (!input) {
    safe_error(error) = "Cannot open input: " + filename;
    return nullptr;
  }
  return std::make_shared<FullHaystack>(filename);
}

std::shared_ptr<Haystack> Haystack::make_stdin(std::string *error) {
  (void)error;
  return std::make_shared<FullHaystack>();
}

} // namespace regexp
} // namespace cottontail
