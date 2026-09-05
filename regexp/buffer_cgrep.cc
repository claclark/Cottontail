#include "regexp/buffer_cgrep.h"
#include "regexp/cgrep_internal.h"
#include "regexp/haystack_cgrep.h"

#include <cstring>
#include <limits>
#include <mutex>

namespace cottontail {
namespace regexp {
namespace {
std::string literal_chain(const std::vector<transition> &transitions,
                          std::size_t state_count) {
  std::vector<const transition *> outgoing(state_count, nullptr);
  for (const transition &tr : transitions) {
    if (outgoing[tr.from] != nullptr || tr.symbols.size() != 1 ||
        *tr.symbols.begin() > 255)
      return "";
    outgoing[tr.from] = &tr;
  }
  std::string literal;
  state from = start_state;
  while (from != final_state) {
    if (literal.size() == transitions.size() || outgoing[from] == nullptr)
      return "";
    const transition &tr = *outgoing[from];
    literal.push_back(static_cast<char>(*tr.symbols.begin()));
    from = tr.to;
  }
  return literal.size() == transitions.size() ? literal : "";
}

const char *reverse_memchr(const char *start, unsigned char byte,
                           std::size_t length) {
#if defined(__GLIBC__)
  return static_cast<const char *>(::memrchr(start, byte, length));
#else
  const char *end = start + length;
  while (end != start)
    if (static_cast<unsigned char>(*--end) == byte)
      return end;
  return nullptr;
#endif
}

} // namespace

struct BufferCgrep::Machine final {
  std::string literal;
  std::size_t step = 1;
};

std::shared_ptr<const BufferCgrep::Machine>
BufferCgrep::machine(std::shared_ptr<const Cgrep::Machine> bundle) {
  std::lock_guard<std::mutex> lock(bundle->mutex);
  if (bundle->buffer != nullptr)
    return std::static_pointer_cast<const Machine>(bundle->buffer);
  auto machine = std::make_shared<Machine>();
  if (bundle->springy)
    machine->literal = literal_chain(bundle->transitions, bundle->state_count);
  const auto &literal = machine->literal;
  while (machine->step < literal.size() &&
         literal.compare(machine->step, literal.size() - machine->step, literal,
                         0, literal.size() - machine->step) != 0)
    machine->step++;
  bundle->buffer = machine;
  return machine;
}

std::shared_ptr<Cgrep>
BufferCgrep::make(std::shared_ptr<const Cgrep::Machine> bundle,
                  std::shared_ptr<const char> buffer, std::size_t size,
                  std::string *error) {
  if (bundle == nullptr || buffer == nullptr ||
      size > static_cast<std::size_t>(maxfinity)) {
    safe_error(error) = "BufferCgrep needs a machine and a valid byte buffer";
    return nullptr;
  }
  auto compiled = machine(bundle);
  if (compiled->literal.empty())
    return HaystackCgrep::make(std::move(bundle),
                               Haystack::make(std::move(buffer), size, error),
                               error);
  return std::shared_ptr<Cgrep>(
      new BufferCgrep(std::move(compiled), std::move(buffer), size));
}

BufferCgrep::BufferCgrep(std::shared_ptr<const Machine> machine,
                         std::shared_ptr<const char> buffer, std::size_t size)
    : Cgrep(std::move(buffer), size), machine_(std::move(machine)),
      current_(buffer_.get()), end_(buffer_.get() + size_) {}

bool BufferCgrep::match_(addr *p, addr *q) {
  if (p == nullptr || q == nullptr) {
    fail("Cgrep::match got a null pointer");
    return false;
  }
  if (!error_.empty())
    return false;
  const auto &literal = machine_->literal;
  while (current_ != end_) {
    const char *first = nullptr;
    const char *found = nullptr;
    for (std::size_t i = 0; i < literal.size(); i++) {
      found = static_cast<const char *>(std::memchr(
          current_, static_cast<unsigned char>(literal[i]), end_ - current_));
      if (found == nullptr) {
        current_ = end_;
        return false;
      }
      if (i == 0)
        first = found;
      current_ = found + 1;
    }
    const char *begin = found;
    for (std::size_t i = literal.size() - 1; i > 0; i--)
      begin = reverse_memchr(first, static_cast<unsigned char>(literal[i - 1]),
                             begin - first);
    current_ = begin + 1;
    if (static_cast<std::size_t>(found - begin + 1) == literal.size()) {
      current_ = begin + machine_->step;
      *p = begin - buffer_.get();
      *q = found - buffer_.get();
      return true;
    }
  }
  return false;
}

bool BufferCgrep::translate_(addr p, addr q, const char **start,
                             const char **end) {
  if (start == nullptr || end == nullptr) {
    fail("Cgrep::translate got a null pointer");
    return false;
  }
  if (!error_.empty())
    return false;
  if (p < 0 || q < p || static_cast<std::size_t>(q) >= size_) {
    fail("Translation outside BufferCgrep");
    return false;
  }
  *start = buffer_.get() + p;
  *end = buffer_.get() + q + 1;
  return true;
}

bool BufferCgrep::reset_(std::string *error) {
  (void)error;
  error_.clear();
  current_ = buffer_.get();
  return true;
}

bool BufferCgrep::success_(std::string *error) {
  if (error_.empty())
    return true;
  safe_error(error) = error_;
  return false;
}

void BufferCgrep::fail(const std::string &message) {
  if (error_.empty())
    error_ = message;
}

} // namespace regexp
} // namespace cottontail
