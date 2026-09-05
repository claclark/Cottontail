#include "regexp/cgrep.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>

#include "regexp/buffer_cgrep.h"
#include "regexp/cgrep_internal.h"
#include "regexp/haystack_cgrep.h"

namespace cottontail {
namespace regexp {
namespace {
constexpr std::size_t number_of_symbols =
    static_cast<symbol>(special_symbol::END) + 1;
// Input selection only: Haystack's storage policy is independent of this cap.
constexpr std::size_t buffer_file_limit = 64 * 1024 * 1024;
} // namespace

std::shared_ptr<const Cgrep::Machine>
Cgrep::compile(const std::string &expression, std::string *error,
               bool springy) {
  std::string cause;
  std::vector<transition> transitions = regexp::nfa(expression, &cause);
  if (transitions.empty()) {
    safe_error(error) = cause.empty() ? "Cannot compile empty NFA" : cause;
    return nullptr;
  }
  return compile(transitions, error, springy);
}

std::shared_ptr<const Cgrep::Machine>
Cgrep::compile(const std::vector<transition> &transitions, std::string *error,
               bool springy) {
  if (transitions.empty()) {
    safe_error(error) = "Cannot compile empty NFA";
    return nullptr;
  }

  state maximum = start_state;
  for (const transition &tr : transitions) {
    if (tr.from == final_state) {
      safe_error(error) = "NFA has a transition from its final state";
      return nullptr;
    }
    maximum = std::max(maximum, tr.from);
    if (tr.to != final_state)
      maximum = std::max(maximum, tr.to);
    if (tr.symbols.empty()) {
      safe_error(error) = "NFA has an empty transition label";
      return nullptr;
    }
    for (symbol value : tr.symbols)
      if (value >= number_of_symbols) {
        safe_error(error) = "NFA transition has an invalid symbol";
        return nullptr;
      }
  }
  if (maximum >= std::numeric_limits<std::size_t>::max() / number_of_symbols) {
    safe_error(error) = "NFA state space is too large";
    return nullptr;
  }

  auto machine = std::make_shared<Machine>();
  machine->transitions = transitions;
  machine->state_count = maximum + 1;
  machine->springy = springy;
  return machine;
}

std::shared_ptr<Cgrep> Cgrep::make(std::shared_ptr<const Machine> machine,
                                   std::shared_ptr<Haystack> haystack,
                                   std::string *error) {
  return HaystackCgrep::make(std::move(machine), std::move(haystack), error);
}

std::shared_ptr<Cgrep> Cgrep::make(std::shared_ptr<const Machine> machine,
                                   std::shared_ptr<const char> buffer,
                                   std::size_t size, std::string *error) {
  return BufferCgrep::make(std::move(machine), std::move(buffer), size, error);
}

std::shared_ptr<Cgrep> Cgrep::make(std::shared_ptr<const Machine> machine,
                                   const char *buffer, std::size_t size,
                                   std::string *error) {
  if (machine == nullptr || (buffer == nullptr && size != 0) ||
      size > static_cast<std::size_t>(maxfinity)) {
    safe_error(error) = "Cgrep needs a machine and a valid byte buffer";
    return nullptr;
  }
  std::shared_ptr<char> storage(new char[std::max<std::size_t>(size, 1)],
                                std::default_delete<char[]>());
  if (size != 0)
    std::memcpy(storage.get(), buffer, size);
  return make(std::move(machine), std::shared_ptr<const char>(storage), size,
              error);
}

std::shared_ptr<Cgrep> Cgrep::make(std::shared_ptr<const Machine> machine,
                                   std::shared_ptr<std::istream> input,
                                   std::string *error) {
  if (machine == nullptr) {
    safe_error(error) = "Cgrep needs a compiled machine";
    return nullptr;
  }
  auto haystack = Haystack::make(std::move(input), error);
  if (haystack == nullptr)
    return nullptr;
  return make(std::move(machine), std::move(haystack), error);
}

std::shared_ptr<Cgrep> Cgrep::make(std::shared_ptr<const Machine> machine,
                                   const std::string &filename,
                                   std::string *error) {
  if (machine == nullptr) {
    safe_error(error) = "Cgrep needs a compiled machine";
    return nullptr;
  }
  std::error_code ec;
  bool regular = std::filesystem::is_regular_file(filename, ec);
  std::uintmax_t size = 0;
  if (regular)
    size = std::filesystem::file_size(filename, ec);
  if (ec) {
    safe_error(error) =
        "Cannot inspect input: " + filename + ": " + ec.message();
    return nullptr;
  }
  if (regular && size > buffer_file_limit) {
    auto haystack = Haystack::make(filename, error);
    if (haystack == nullptr)
      return nullptr;
    return make(std::move(machine), std::move(haystack), error);
  }
  auto input = std::make_shared<std::ifstream>(filename, std::ios::binary);
  if (!*input) {
    safe_error(error) = "Cannot open input: " + filename;
    return nullptr;
  }
  if (!regular)
    return make(std::move(machine),
                std::static_pointer_cast<std::istream>(input), error);

  std::shared_ptr<char> buffer(
      new char[std::max<std::size_t>(static_cast<std::size_t>(size), 1)],
      std::default_delete<char[]>());
  input->read(buffer.get(), static_cast<std::streamsize>(size));
  if (input->bad() || (input->fail() && !input->eof())) {
    safe_error(error) = "Cannot read input: " + filename;
    return nullptr;
  }
  return make(std::move(machine), std::shared_ptr<const char>(buffer),
              static_cast<std::size_t>(input->gcount()), error);
}

std::shared_ptr<Cgrep> Cgrep::make(const std::string &expression,
                                   std::shared_ptr<Haystack> haystack,
                                   std::string *error) {
  std::shared_ptr<const Machine> machine = compile(expression, error);
  if (machine == nullptr)
    return nullptr;
  return make(std::move(machine), std::move(haystack), error);
}

std::shared_ptr<Cgrep> Cgrep::make(const std::vector<transition> &transitions,
                                   std::shared_ptr<Haystack> haystack,
                                   std::string *error) {
  std::shared_ptr<const Machine> machine = compile(transitions, error);
  if (machine == nullptr)
    return nullptr;
  return make(std::move(machine), std::move(haystack), error);
}

std::string Cgrep::translate(addr p, addr q) {
  const char *start;
  const char *end;
  if (!translate(p, q, &start, &end))
    return "";
  return std::string(start, end);
}

} // namespace regexp
} // namespace cottontail
