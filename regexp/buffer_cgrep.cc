#include "regexp/buffer_cgrep.h"
#include "regexp/haystack_cgrep.h"

#include <cstring>
#include <limits>

namespace cottontail {
namespace regexp {
namespace {
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

std::shared_ptr<Cgrep>
BufferCgrep::make(std::shared_ptr<const Cgrep::Machine> bundle,
                  std::shared_ptr<const char> buffer, std::size_t size,
                  std::string *error) {
  if (bundle == nullptr || buffer == nullptr ||
      size > static_cast<std::size_t>(maxfinity)) {
    safe_error(error) = "BufferCgrep needs a machine and a valid byte buffer";
    return nullptr;
  }
  auto compiled = bundle->buffer_machines().front();
  if (compiled->kind == BufferMachine::Kind::STANDARD)
    return HaystackCgrep::make(std::move(bundle),
                               Haystack::make(std::move(buffer), size, error),
                               error);
  return std::shared_ptr<Cgrep>(
      new BufferCgrep(std::move(compiled), std::move(buffer), size));
}

BufferCgrep::BufferCgrep(std::shared_ptr<const BufferMachine> machine,
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

BufferLineCgrep::BufferLineCgrep(std::shared_ptr<Cgrep> raw, std::size_t limit)
    : raw(std::move(raw)), line_limit(limit) {}

void BufferLineCgrep::advance(Line *line, addr position) {
  const char *buffer = raw->buffer_.get();
  while (position > line->q) {
    if (line->q >= 0) {
      line->p = line->q + 1;
      line->number++;
    }
    const char *lf = static_cast<const char *>(
        std::memchr(buffer + line->p, '\n', raw->size_ - line->p));
    line->q = lf == nullptr ? static_cast<addr>(raw->size_) - 1 : lf - buffer;
  }
}

bool BufferLineCgrep::match_(LineCgrep::Match *answer) {
  if (answer == nullptr) {
    fail("LineCgrep::match got a null pointer");
    return false;
  }
  if (!error.empty())
    return false;
  addr p;
  addr q;
  if (!raw->match(&p, &q))
    return false;
  advance(&first, p);
  advance(&last, q);
  *answer = LineCgrep::Match{p, q, 0, 0, 0, 0, 0, 0, false};
  if (line_limit == 0 || last.number - first.number + 1 <= line_limit) {
    answer->lines_p = first.p;
    answer->lines_q = last.q;
    answer->start_line = first.number;
    answer->start_position = static_cast<std::size_t>(p - first.p) + 1;
    answer->end_line = last.number;
    answer->end_position = static_cast<std::size_t>(q - last.p) + 1;
    answer->has_lines = true;
  }
  return true;
}

bool BufferLineCgrep::translate_(const LineCgrep::Match &match,
                                 const char **start, const char **end) {
  if (!error.empty())
    return false;
  if (!match.has_lines) {
    fail("LineCgrep match has no retained line text");
    return false;
  }
  return raw->translate(match.lines_p, match.lines_q, start, end);
}

bool BufferLineCgrep::reset_(std::string *message) {
  if (!raw->reset(message))
    return false;
  first = Line{};
  last = Line{};
  error.clear();
  return true;
}

bool BufferLineCgrep::success_(std::string *message) {
  if (!error.empty()) {
    safe_error(message) = error;
    return false;
  }
  return raw->success(message);
}

void BufferLineCgrep::fail(const std::string &message) {
  if (error.empty())
    error = message;
}

std::shared_ptr<LineCgrep> BufferLineCgrep::make(std::shared_ptr<Cgrep> raw,
                                                 std::size_t lines,
                                                 std::string *error) {
  if (raw == nullptr || raw->buffer_ == nullptr) {
    safe_error(error) = "BufferLineCgrep needs a buffer matcher";
    return nullptr;
  }
  return std::shared_ptr<LineCgrep>(new BufferLineCgrep(std::move(raw), lines));
}

} // namespace regexp
} // namespace cottontail
