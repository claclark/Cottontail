#include "regexp/machine.h"

#include <algorithm>

namespace cottontail {
namespace regexp {
namespace {
constexpr std::size_t number_of_symbols =
    static_cast<symbol>(special_symbol::END) + 1;

std::shared_ptr<const HaystackMachine>
compile_haystack(const std::vector<transition> &transitions,
                 std::size_t state_count) {
  auto machine = std::make_shared<HaystackMachine>();
  machine->state_count = state_count;
  std::vector<std::vector<state>> lists(machine->state_count *
                                        number_of_symbols);
  for (const transition &tr : transitions)
    for (symbol value : tr.symbols)
      lists[tr.from * number_of_symbols + value].push_back(tr.to);

  machine->dispatch.reserve(lists.size());
  for (std::vector<state> &destinations : lists) {
    std::sort(destinations.begin(), destinations.end());
    destinations.erase(std::unique(destinations.begin(), destinations.end()),
                       destinations.end());
    HaystackMachine::Cell cell{machine->destinations.size(),
                               machine->destinations.size()};
    machine->destinations.insert(machine->destinations.end(),
                                 destinations.begin(), destinations.end());
    cell.end = machine->destinations.size();
    machine->dispatch.push_back(cell);
  }
  return machine;
}

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

} // namespace

std::shared_ptr<const HaystackMachine> Machine::haystack_machine() const {
  std::lock_guard<std::mutex> lock(mutex);
  if (haystack == nullptr)
    haystack = compile_haystack(transitions, state_count);
  return haystack;
}

std::vector<std::shared_ptr<const BufferMachine>>
Machine::buffer_machines() const {
  std::lock_guard<std::mutex> lock(mutex);
  if (buffer.empty()) {
    auto machine = std::make_shared<BufferMachine>();
    machine->literal = literal_chain(transitions, state_count);
    if (machine->literal.empty()) {
      if (haystack == nullptr)
        haystack = compile_haystack(transitions, state_count);
      machine->standard = haystack;
    } else {
      machine->kind = BufferMachine::Kind::PHRASE;
      const auto &literal = machine->literal;
      while (machine->step < literal.size() &&
             literal.compare(machine->step, literal.size() - machine->step,
                             literal, 0, literal.size() - machine->step) != 0)
        machine->step++;
    }
    buffer.push_back(machine);
  }
  return buffer;
}

} // namespace regexp
} // namespace cottontail
