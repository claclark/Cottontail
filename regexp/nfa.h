#ifndef COTTONTAIL_REGEXP_NFA_H_
#define COTTONTAIL_REGEXP_NFA_H_

#include <cstddef>
#include <cstdint>
#include <limits>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace cottontail {
namespace regexp {

using state = std::size_t;
using symbol = std::uint16_t;

constexpr state start_state = 0;
constexpr state final_state = std::numeric_limits<state>::max();

enum class special_symbol : symbol {
  START = 256,
  END,
};

// A transition accepts one of its symbols. Byte symbols have values 0 through
// 255; the remaining symbols describe positions outside the text buffer. The
// start and final states use the constants above.
struct transition {
  state from;
  state to;
  std::set<symbol> symbols;
};

std::vector<transition> nfa(const std::string &regexp, std::string *error);

// Return the inclusive byte intervals of the shortest matching substrings.
// START and END refer to the boundaries of text. Matches may overlap.
std::vector<std::pair<std::size_t, std::size_t>>
match(const std::vector<transition> &machine, const std::string &text);

} // namespace regexp
} // namespace cottontail

#endif // COTTONTAIL_REGEXP_NFA_H_
