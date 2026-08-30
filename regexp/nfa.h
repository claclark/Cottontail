#ifndef COTTONTAIL_REGEXP_NFA_H_
#define COTTONTAIL_REGEXP_NFA_H_

#include <cstddef>
#include <limits>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace cottontail {
namespace regexp {

using state = std::size_t;

constexpr state start_state = 0;
constexpr state final_state = std::numeric_limits<state>::max();

// A transition accepts bytes in characters, or bytes not in characters when
// complement is true. The start and final states use the constants above.
struct transition {
  state from;
  state to;
  bool complement;
  std::set<unsigned char> characters;
};

std::vector<transition> nfa(const std::string &regexp, std::string *error);

// Return the inclusive byte intervals of the shortest matching substrings.
// Matches may overlap.
std::vector<std::pair<std::size_t, std::size_t>>
match(const std::vector<transition> &machine, const std::string &text);

} // namespace regexp
} // namespace cottontail

#endif // COTTONTAIL_REGEXP_NFA_H_
