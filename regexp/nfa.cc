#include "regexp/nfa.h"

#include <algorithm>
#include <array>
#include <map>
#include <queue>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "src/core.h"

namespace cottontail {
namespace regexp {
namespace {

struct expression {
  bool lambda = false;
  std::vector<transition> transitions;
};

bool accepts(const transition &tr, unsigned char c) {
  bool contains = tr.characters.find(c) != tr.characters.end();
  return tr.complement ? !contains : contains;
}

std::array<bool, 256> members(const transition &tr) {
  std::array<bool, 256> answer;
  answer.fill(tr.complement);
  for (unsigned char c : tr.characters)
    answer[c] = !tr.complement;
  return answer;
}

transition make_transition(state from, state to,
                           const std::array<bool, 256> &membership) {
  transition answer{from, to, false, {}};
  std::size_t included = 0;
  for (bool present : membership)
    if (present)
      included++;
  answer.complement = included > 128;
  for (std::size_t i = 0; i < membership.size(); i++)
    if (membership[i] != answer.complement)
      answer.characters.insert(static_cast<unsigned char>(i));
  return answer;
}

bool empty(const transition &tr) {
  if (!tr.complement)
    return tr.characters.empty();
  return tr.characters.size() == 256;
}

state maximum_state(const std::vector<transition> &machine) {
  state maximum = start_state;
  for (const transition &tr : machine) {
    maximum = std::max(maximum, tr.from);
    if (tr.to != final_state)
      maximum = std::max(maximum, tr.to);
  }
  return maximum;
}

void add_label(std::array<bool, 256> *label, const transition &tr) {
  std::array<bool, 256> addition = members(tr);
  for (std::size_t i = 0; i < label->size(); i++)
    (*label)[i] = (*label)[i] || addition[i];
}

std::vector<transition> normalize(std::vector<transition> machine) {
  std::map<std::pair<state, state>, std::array<bool, 256>> labels;
  for (const transition &tr : machine) {
    if (empty(tr))
      continue;
    auto key = std::make_pair(tr.from, tr.to);
    auto found = labels.find(key);
    if (found == labels.end()) {
      std::array<bool, 256> label;
      label.fill(false);
      found = labels.emplace(key, label).first;
    }
    add_label(&found->second, tr);
  }

  std::set<state> useful;
  for (const auto &item : labels)
    if (item.first.second == final_state)
      useful.insert(item.first.first);
  bool changed;
  do {
    changed = false;
    for (const auto &item : labels)
      if (item.first.second != final_state &&
          useful.find(item.first.second) != useful.end() &&
          useful.insert(item.first.first).second)
        changed = true;
  } while (changed);

  std::set<state> reachable{start_state};
  do {
    changed = false;
    for (const auto &item : labels)
      if (useful.find(item.first.first) != useful.end() &&
          reachable.find(item.first.first) != reachable.end() &&
          item.first.second != final_state &&
          reachable.insert(item.first.second).second)
        changed = true;
  } while (changed);

  std::set<state> states{start_state};
  for (const auto &item : labels)
    if (useful.find(item.first.first) != useful.end() &&
        reachable.find(item.first.first) != reachable.end()) {
      states.insert(item.first.first);
      if (item.first.second != final_state)
        states.insert(item.first.second);
    }
  std::map<state, state> compact;
  compact[start_state] = start_state;
  state next = 1;
  for (state old : states)
    if (old != start_state)
      compact[old] = next++;

  std::vector<transition> answer;
  for (const auto &item : labels) {
    state from = item.first.first;
    state to = item.first.second;
    if (useful.find(from) == useful.end() ||
        reachable.find(from) == reachable.end())
      continue;
    if (to != final_state &&
        (useful.find(to) == useful.end() ||
         reachable.find(to) == reachable.end()))
      continue;
    answer.push_back(make_transition(compact[from],
                                     to == final_state ? final_state
                                                       : compact[to],
                                     item.second));
  }
  std::sort(answer.begin(), answer.end(),
            [](const transition &left, const transition &right) {
              if (left.from != right.from)
                return left.from > right.from;
              if (left.to != right.to)
                return left.to > right.to;
              if (left.complement != right.complement)
                return left.complement < right.complement;
              return left.characters < right.characters;
            });
  return answer;
}

std::vector<transition>
unite_nonempty(const std::vector<transition> &left,
               const std::vector<transition> &right) {
  if (left.empty())
    return right;
  if (right.empty())
    return left;
  std::vector<transition> answer = left;
  state offset = maximum_state(left);
  for (transition tr : right) {
    if (tr.from != start_state)
      tr.from += offset;
    if (tr.to != start_state && tr.to != final_state)
      tr.to += offset;
    answer.push_back(std::move(tr));
  }
  return normalize(std::move(answer));
}

expression unite(expression left, expression right) {
  expression answer;
  answer.lambda = left.lambda || right.lambda;
  answer.transitions = unite_nonempty(left.transitions, right.transitions);
  return answer;
}

std::vector<transition>
concatenate_nonempty(const std::vector<transition> &left,
                     const std::vector<transition> &right) {
  if (left.empty() || right.empty())
    return {};
  state join = maximum_state(left) + 1;
  std::vector<transition> answer;
  answer.reserve(left.size() + right.size());
  for (transition tr : left) {
    if (tr.to == final_state)
      tr.to = join;
    answer.push_back(std::move(tr));
  }
  for (transition tr : right) {
    tr.from = tr.from == start_state ? join : tr.from + join;
    if (tr.to != final_state)
      tr.to = tr.to == start_state ? join : tr.to + join;
    answer.push_back(std::move(tr));
  }
  return normalize(std::move(answer));
}

expression concatenate(expression left, expression right) {
  expression answer;
  answer.lambda = left.lambda && right.lambda;
  answer.transitions =
      concatenate_nonempty(left.transitions, right.transitions);
  if (left.lambda)
    answer.transitions =
        unite_nonempty(answer.transitions, right.transitions);
  if (right.lambda)
    answer.transitions = unite_nonempty(answer.transitions, left.transitions);
  return answer;
}

expression closure(expression value, char kind) {
  std::vector<transition> repeated = value.transitions;
  if (kind != '?')
    for (const transition &tr : value.transitions)
      if (tr.to == final_state) {
        transition restart = tr;
        restart.to = start_state;
        repeated.push_back(std::move(restart));
      }
  expression answer;
  answer.lambda = kind != '+' || value.lambda;
  answer.transitions = normalize(std::move(repeated));
  return answer;
}

bool intersect_label(const transition &left, const transition &right,
                     std::array<bool, 256> *label) {
  std::array<bool, 256> a = members(left);
  std::array<bool, 256> b = members(right);
  bool any = false;
  for (std::size_t i = 0; i < label->size(); i++) {
    (*label)[i] = a[i] && b[i];
    any = any || (*label)[i];
  }
  return any;
}

expression intersect(expression left, expression right) {
  expression answer;
  answer.lambda = left.lambda && right.lambda;
  if (left.transitions.empty() || right.transitions.empty())
    return answer;

  using pair = std::pair<state, state>;
  std::map<pair, state> states;
  std::queue<pair> pending;
  pair start{start_state, start_state};
  states[start] = start_state;
  pending.push(start);
  state next_state = 1;

  while (!pending.empty()) {
    pair current = pending.front();
    pending.pop();
    for (const transition &a : left.transitions) {
      if (a.from != current.first)
        continue;
      for (const transition &b : right.transitions) {
        if (b.from != current.second)
          continue;
        std::array<bool, 256> label;
        if (!intersect_label(a, b, &label))
          continue;
        if ((a.to == final_state) != (b.to == final_state))
          continue;
        state destination;
        if (a.to == final_state) {
          destination = final_state;
        } else {
          pair target{a.to, b.to};
          auto found = states.find(target);
          if (found == states.end()) {
            found = states.emplace(target, next_state++).first;
            pending.push(target);
          }
          destination = found->second;
        }
        answer.transitions.push_back(
            make_transition(states[current], destination, label));
      }
    }
  }
  answer.transitions = normalize(std::move(answer.transitions));
  return answer;
}

expression symbol(const std::array<bool, 256> &label) {
  expression answer;
  answer.transitions.push_back(
      make_transition(start_state, final_state, label));
  return answer;
}

std::array<bool, 256> singleton(unsigned char c) {
  std::array<bool, 256> label;
  label.fill(false);
  label[c] = true;
  return label;
}

void include_range(std::array<bool, 256> *label, unsigned char first,
                   unsigned char last) {
  for (unsigned int c = first; c <= last; c++)
    (*label)[c] = true;
}

class parser final {
public:
  parser(const std::string &input, std::string *error)
      : input_(input), error_(error) {}

  expression parse() {
    expression answer = parse_union();
    if (!failed_ && where_ != input_.size())
      fail("Unexpected character");
    return answer;
  }

  bool failed() const { return failed_; }

private:
  expression parse_union() {
    expression answer = parse_intersection();
    while (!failed_ && take('|'))
      answer = unite(std::move(answer), parse_intersection());
    return answer;
  }

  expression parse_intersection() {
    expression answer = parse_concatenation();
    while (!failed_ && take('&'))
      answer = intersect(std::move(answer), parse_concatenation());
    return answer;
  }

  expression parse_concatenation() {
    expression answer;
    answer.lambda = true;
    while (!failed_ && where_ < input_.size() && input_[where_] != ')' &&
           input_[where_] != '|' && input_[where_] != '&')
      answer = concatenate(std::move(answer), parse_repetition());
    return answer;
  }

  expression parse_repetition() {
    expression answer = parse_atom();
    if (failed_ || where_ == input_.size())
      return answer;
    char c = input_[where_];
    if (c == '*' || c == '+' || c == '?') {
      where_++;
      answer = closure(std::move(answer), c);
      if (where_ < input_.size() &&
          (input_[where_] == '*' || input_[where_] == '+' ||
           input_[where_] == '?'))
        fail("Repeated quantifier");
    }
    return answer;
  }

  expression parse_atom() {
    if (where_ == input_.size()) {
      fail("Expected an expression");
      return {};
    }
    unsigned char c = byte(where_++);
    if (c == '(') {
      expression answer = parse_union();
      if (!take(')'))
        fail("Expected ')'");
      return answer;
    }
    if (c == '[')
      return parse_class();
    if (c == '.') {
      std::array<bool, 256> label;
      label.fill(true);
      return symbol(label);
    }
    if (c == '\\') {
      std::array<bool, 256> label;
      if (!parse_escape(&label))
        return {};
      return symbol(label);
    }
    if (c == '*' || c == '+' || c == '?') {
      fail("Quantifier has no expression");
      return {};
    }
    if (c == '{') {
      fail("Counted repetition is not supported");
      return {};
    }
    if (c == '^' || c == '$') {
      fail("Anchors are not supported");
      return {};
    }
    return symbol(singleton(c));
  }

  expression parse_class() {
    bool complement = take('^');
    std::array<bool, 256> label;
    label.fill(false);
    bool any = false;
    while (!failed_ && where_ < input_.size() && input_[where_] != ']') {
      std::array<bool, 256> first;
      int first_byte = -1;
      if (!parse_class_item(&first, &first_byte))
        return {};
      any = true;
      if (where_ < input_.size() && input_[where_] == '-' &&
          where_ + 1 < input_.size() && input_[where_ + 1] != ']') {
        where_++;
        std::array<bool, 256> last;
        int last_byte = -1;
        if (!parse_class_item(&last, &last_byte))
          return {};
        if (first_byte < 0 || last_byte < 0 || first_byte > last_byte) {
          fail("Invalid character range");
          return {};
        }
        include_range(&label, static_cast<unsigned char>(first_byte),
                      static_cast<unsigned char>(last_byte));
      } else {
        for (std::size_t i = 0; i < label.size(); i++)
          label[i] = label[i] || first[i];
      }
    }
    if (!take(']')) {
      fail("Expected ']'");
      return {};
    }
    if (!any) {
      fail("Empty character class");
      return {};
    }
    if (complement)
      for (std::size_t i = 0; i < label.size(); i++)
        label[i] = !label[i];
    return symbol(label);
  }

  bool parse_class_item(std::array<bool, 256> *label, int *single) {
    label->fill(false);
    *single = -1;
    if (where_ == input_.size()) {
      fail("Expected ']'");
      return false;
    }
    unsigned char c = byte(where_++);
    if (c == '\\') {
      if (!parse_escape(label))
        return false;
      int found = -1;
      for (std::size_t i = 0; i < label->size(); i++)
        if ((*label)[i]) {
          if (found != -1)
            return true;
          found = static_cast<int>(i);
        }
      *single = found;
      return true;
    }
    (*label)[c] = true;
    *single = c;
    return true;
  }

  bool parse_escape(std::array<bool, 256> *label) {
    label->fill(false);
    if (where_ == input_.size()) {
      fail("Trailing backslash");
      return false;
    }
    unsigned char c = byte(where_++);
    switch (c) {
    case 'n':
      (*label)['\n'] = true;
      break;
    case 'r':
      (*label)['\r'] = true;
      break;
    case 't':
      (*label)['\t'] = true;
      break;
    case 'f':
      (*label)['\f'] = true;
      break;
    case 'v':
      (*label)['\v'] = true;
      break;
    case 'x': {
      if (where_ + 2 > input_.size() || hex(input_[where_]) < 0 ||
          hex(input_[where_ + 1]) < 0) {
        fail("Expected two hexadecimal digits after \\x");
        return false;
      }
      unsigned char value = static_cast<unsigned char>(
          16 * hex(input_[where_]) + hex(input_[where_ + 1]));
      where_ += 2;
      (*label)[value] = true;
      break;
    }
    case 'd':
      include_range(label, '0', '9');
      break;
    case 's':
      (*label)[' '] = true;
      (*label)['\t'] = true;
      (*label)['\n'] = true;
      (*label)['\r'] = true;
      (*label)['\f'] = true;
      (*label)['\v'] = true;
      break;
    case 'w':
      include_range(label, '0', '9');
      include_range(label, 'A', 'Z');
      include_range(label, 'a', 'z');
      (*label)['_'] = true;
      break;
    case 'D':
    case 'S':
    case 'W': {
      unsigned char lower = static_cast<unsigned char>(c - 'A' + 'a');
      if (lower == 'd')
        include_range(label, '0', '9');
      else if (lower == 's') {
        (*label)[' '] = true;
        (*label)['\t'] = true;
        (*label)['\n'] = true;
        (*label)['\r'] = true;
        (*label)['\f'] = true;
        (*label)['\v'] = true;
      } else {
        include_range(label, '0', '9');
        include_range(label, 'A', 'Z');
        include_range(label, 'a', 'z');
        (*label)['_'] = true;
      }
      for (std::size_t i = 0; i < label->size(); i++)
        (*label)[i] = !(*label)[i];
      break;
    }
    default:
      (*label)[c] = true;
      break;
    }
    return true;
  }

  bool take(char c) {
    if (where_ < input_.size() && input_[where_] == c) {
      where_++;
      return true;
    }
    return false;
  }

  unsigned char byte(std::size_t where) const {
    return static_cast<unsigned char>(input_[where]);
  }

  static int hex(char c) {
    if (c >= '0' && c <= '9')
      return c - '0';
    if (c >= 'a' && c <= 'f')
      return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
      return c - 'A' + 10;
    return -1;
  }

  void fail(const std::string &message) {
    if (!failed_) {
      safe_error(error_) = message + " at byte " + std::to_string(where_);
      failed_ = true;
    }
  }

  const std::string &input_;
  std::string *error_;
  std::size_t where_ = 0;
  bool failed_ = false;
};

} // namespace

std::vector<transition> nfa(const std::string &regexp, std::string *error) {
  safe_set(error).clear();
  parser compiler(regexp, error);
  expression result = compiler.parse();
  if (compiler.failed())
    return {};
  if (result.lambda) {
    safe_error(error) = "Regular expression matches lambda";
    return {};
  }
  result.transitions = normalize(std::move(result.transitions));
  if (result.transitions.empty()) {
    safe_error(error) = "Regular expression matches no strings";
    return {};
  }
  return result.transitions;
}

std::vector<std::pair<std::size_t, std::size_t>>
match(const std::vector<transition> &machine, const std::string &text) {
  std::unordered_map<state, std::size_t> current;
  std::vector<std::pair<std::size_t, std::size_t>> candidates;
  for (std::size_t end = 0; end < text.size(); end++) {
    unsigned char c = static_cast<unsigned char>(text[end]);
    std::unordered_map<state, std::size_t> next;
    bool accepted = false;
    std::size_t accepted_start = 0;
    auto advance = [&](const transition &tr, std::size_t start) {
      if (tr.to == final_state) {
        if (!accepted || start > accepted_start) {
          accepted = true;
          accepted_start = start;
        }
      } else {
        auto found = next.find(tr.to);
        if (found == next.end() || start > found->second)
          next[tr.to] = start;
      }
    };
    for (const transition &tr : machine) {
      if (!accepts(tr, c))
        continue;
      if (tr.from == start_state)
        advance(tr, end);
      auto found = current.find(tr.from);
      if (found != current.end())
        advance(tr, found->second);
    }
    if (accepted)
      candidates.emplace_back(accepted_start, end);
    current = std::move(next);
  }

  std::vector<std::pair<std::size_t, std::size_t>> answer;
  bool have_start = false;
  std::size_t largest_start = 0;
  for (const auto &candidate : candidates) {
    if (!have_start || candidate.first > largest_start)
      answer.push_back(candidate);
    if (!have_start || candidate.first > largest_start) {
      largest_start = candidate.first;
      have_start = true;
    }
  }
  return answer;
}

} // namespace regexp
} // namespace cottontail
