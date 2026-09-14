#ifndef COTTONTAIL_REGEXP_MACHINE_H_
#define COTTONTAIL_REGEXP_MACHINE_H_

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "regexp/nfa.h"

namespace cottontail {
namespace regexp {

struct HaystackMachine {
  struct Cell {
    std::size_t begin;
    std::size_t end;
  };
  std::size_t state_count;
  std::vector<Cell> dispatch;
  std::vector<state> destinations;
};

struct BufferMachine {
  enum class Kind { STANDARD, PHRASE };
  Kind kind = Kind::STANDARD;
  std::shared_ptr<const HaystackMachine> standard;
  std::string literal;
  std::size_t step = 1;
};

// Shared compilation record. Published representations are immutable;
// callers fetch them once, then match without holding the cache mutex.
struct Machine {
  std::vector<transition> transitions;
  std::size_t state_count = 0;
  mutable std::mutex mutex;
  mutable std::shared_ptr<const HaystackMachine> haystack;
  // Empty until compiled. One component for now; follow decomposition is later.
  mutable std::vector<std::shared_ptr<const BufferMachine>> buffer;

  std::shared_ptr<const HaystackMachine> haystack_machine() const;
  std::vector<std::shared_ptr<const BufferMachine>> buffer_machines() const;
};

} // namespace regexp
} // namespace cottontail

#endif // COTTONTAIL_REGEXP_MACHINE_H_
