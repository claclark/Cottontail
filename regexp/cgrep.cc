#include "regexp/cgrep.h"

#include <algorithm>
#include <cstddef>
#include <deque>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace cottontail {
namespace regexp {
namespace {

constexpr std::size_t number_of_symbols =
    static_cast<symbol>(special_symbol::END) + 1;
constexpr addr unset = minfinity;

struct Cell {
  std::size_t begin;
  std::size_t end;
};

} // namespace

struct Cgrep::Machine final {
  std::size_t state_count;
  std::vector<Cell> dispatch;
  std::vector<state> destinations;
};

std::shared_ptr<const Cgrep::Machine>
Cgrep::compile(const std::string &expression, std::string *error) {
  std::string cause;
  std::vector<transition> transitions = regexp::nfa(expression, &cause);
  if (transitions.empty()) {
    safe_error(error) = cause.empty() ? "Cannot compile empty NFA" : cause;
    return nullptr;
  }
  return compile(transitions, error);
}

std::shared_ptr<const Cgrep::Machine>
Cgrep::compile(const std::vector<transition> &transitions, std::string *error) {
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

  std::shared_ptr<Machine> machine = std::make_shared<Machine>();
  machine->state_count = maximum + 1;
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
    Cell cell{machine->destinations.size(), machine->destinations.size()};
    machine->destinations.insert(machine->destinations.end(),
                                 destinations.begin(), destinations.end());
    cell.end = machine->destinations.size();
    machine->dispatch.push_back(cell);
  }
  return machine;
}

std::shared_ptr<Cgrep> Cgrep::make(std::shared_ptr<const Machine> machine,
                                   std::shared_ptr<Haystack> haystack,
                                   std::string *error) {
  if (machine == nullptr) {
    safe_error(error) = "Cgrep needs a compiled machine";
    return nullptr;
  }
  if (haystack == nullptr) {
    safe_error(error) = "Cgrep needs a Haystack";
    return nullptr;
  }
  return std::shared_ptr<Cgrep>(
      new Cgrep(std::move(machine), std::move(haystack)));
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

Cgrep::Cgrep(std::shared_ptr<const Machine> machine,
             std::shared_ptr<Haystack> haystack)
    : machine_(std::move(machine)), haystack_(std::move(haystack)) {
  initialize();
}

void Cgrep::initialize() {
  starts_.assign(machine_->state_count, unset);
  next_starts_.assign(machine_->state_count, unset);
  active_.clear();
  next_active_.clear();
  current_ = nullptr;
  end_ = nullptr;
  offset_ = 0;
  largest_start_ = 0;
  pending_limit_ = 0;
  started_ = false;
  ended_ = false;
  have_largest_start_ = false;
  have_pending_limit_ = false;
}

bool Cgrep::consume(symbol value, addr end, addr *accepted_start) {
  bool accepted = false;
  addr best = unset;
  next_active_.clear();

  auto advance = [&](state from, addr start) {
    const Cell &cell = machine_->dispatch[from * number_of_symbols + value];
    for (std::size_t i = cell.begin; i < cell.end; i++) {
      state to = machine_->destinations[i];
      if (to == final_state) {
        if (!accepted || start > best) {
          accepted = true;
          best = start;
        }
      } else if (next_starts_[to] == unset) {
        next_starts_[to] = start;
        next_active_.push_back(to);
      } else if (start > next_starts_[to]) {
        next_starts_[to] = start;
      }
    }
  };

  advance(start_state, end);
  for (state from : active_)
    advance(from, starts_[from]);

  for (state from : active_)
    starts_[from] = unset;
  starts_.swap(next_starts_);
  active_.swap(next_active_);

  if (accepted)
    *accepted_start = best;
  return accepted;
}

void Cgrep::prune(addr p) {
  std::size_t kept = 0;
  for (state current : active_) {
    if (starts_[current] > p) {
      active_[kept++] = current;
    } else {
      starts_[current] = unset;
    }
  }
  active_.resize(kept);
}

bool Cgrep::candidate(addr start, addr end, addr *p, addr *q) {
  addr clean_start = std::max<addr>(start, 0);
  addr clean_end = end;
  if (ended_)
    clean_end = std::min(clean_end, offset_ - 1);
  if (clean_start > clean_end)
    return false;
  if (have_largest_start_ && clean_start <= largest_start_)
    return false;

  largest_start_ = clean_start;
  have_largest_start_ = true;
  prune(clean_start);
  pending_limit_ = clean_start;
  have_pending_limit_ = true;
  *p = clean_start;
  *q = clean_end;
  return true;
}

bool Cgrep::next_chunk() {
  const char *start;
  const char *end;
  if (!haystack_->chunk(&start, &end))
    return false;
  if (start == nullptr || end == nullptr || start >= end) {
    fail("Haystack returned an invalid chunk");
    return false;
  }
  current_ = start;
  end_ = end;
  return true;
}

bool Cgrep::match(addr *p, addr *q) {
  if (p == nullptr || q == nullptr) {
    fail("Cgrep::match got a null pointer");
    return false;
  }
  if (!error_.empty())
    return false;
  if (have_pending_limit_) {
    haystack_->limit(pending_limit_);
    have_pending_limit_ = false;
  }
  if (ended_)
    return false;

  addr accepted_start;
  if (!started_) {
    started_ = true;
    if (consume(static_cast<symbol>(special_symbol::START), -1,
                &accepted_start) &&
        candidate(accepted_start, -1, p, q))
      return true;
  }

  for (;;) {
    if (current_ != end_) {
      addr here = offset_++;
      symbol value = static_cast<unsigned char>(*current_++);
      if (consume(value, here, &accepted_start) &&
          candidate(accepted_start, here, p, q))
        return true;
      if (active_.empty())
        haystack_->limit(here);
      continue;
    }

    if (next_chunk())
      continue;
    if (!error_.empty())
      return false;
    std::string cause;
    if (!haystack_->success(&cause)) {
      fail(cause);
      return false;
    }

    ended_ = true;
    if (consume(static_cast<symbol>(special_symbol::END), offset_,
                &accepted_start) &&
        candidate(accepted_start, offset_, p, q))
      return true;
    return false;
  }
}

std::string Cgrep::translate(addr p, addr q) {
  const char *start;
  const char *end;
  if (!translate(p, q, &start, &end))
    return "";
  return std::string(start, end);
}

bool Cgrep::translate(addr p, addr q, const char **start, const char **end) {
  if (!error_.empty())
    return false;
  if (haystack_->translate(p, q, start, end))
    return true;
  std::string cause;
  if (!haystack_->success(&cause))
    fail(cause);
  else
    fail("Haystack could not translate interval");
  return false;
}

bool Cgrep::reset(std::string *error) {
  if (!haystack_->reset(error))
    return false;
  error_.clear();
  initialize();
  return true;
}

bool Cgrep::success(std::string *error) {
  if (!error_.empty()) {
    safe_error(error) = error_;
    return false;
  }
  return haystack_->success(error);
}

void Cgrep::fail(const std::string &message) {
  if (error_.empty())
    error_ = message;
}

struct LineCgrep::Impl final {
  struct Line {
    addr p;
    addr q;
    std::size_t number;
  };

  struct Pending {
    addr p;
    addr q;
  };

  Impl(std::shared_ptr<const Cgrep::Machine> machine,
       std::shared_ptr<Haystack> haystack, std::size_t limit)
      : machine(std::move(machine)), haystack(std::move(haystack)),
        line_limit(limit) {
    initialize();
  }

  void initialize() {
    starts.assign(machine->state_count, unset);
    next_starts.assign(machine->state_count, unset);
    active.clear();
    next_active.clear();
    lines.clear();
    pending.clear();
    ready.clear();
    current = nullptr;
    end = nullptr;
    offset = 0;
    line_p = 0;
    line_number = 1;
    largest_start = 0;
    started = false;
    ended = false;
    have_largest_start = false;
  }

  bool consume(symbol value, addr finish, addr *accepted_start) {
    bool accepted = false;
    addr best = unset;
    next_active.clear();

    auto advance = [&](state from, addr start) {
      const Cell &cell = machine->dispatch[from * number_of_symbols + value];
      for (std::size_t i = cell.begin; i < cell.end; i++) {
        state to = machine->destinations[i];
        if (to == final_state) {
          if (!accepted || start > best) {
            accepted = true;
            best = start;
          }
        } else if (next_starts[to] == unset) {
          next_starts[to] = start;
          next_active.push_back(to);
        } else if (start > next_starts[to]) {
          next_starts[to] = start;
        }
      }
    };

    advance(start_state, finish);
    for (state from : active)
      advance(from, starts[from]);

    for (state from : active)
      starts[from] = unset;
    starts.swap(next_starts);
    active.swap(next_active);

    if (accepted)
      *accepted_start = best;
    return accepted;
  }

  void prune(addr p) {
    std::size_t kept = 0;
    for (state current : active) {
      if (starts[current] > p) {
        active[kept++] = current;
      } else {
        starts[current] = unset;
      }
    }
    active.resize(kept);
  }

  void candidate(addr start, addr finish) {
    addr clean_start = std::max<addr>(start, 0);
    addr clean_end = finish;
    if (ended)
      clean_end = std::min(clean_end, offset - 1);
    if (clean_start > clean_end)
      return;
    if (have_largest_start && clean_start <= largest_start)
      return;

    largest_start = clean_start;
    have_largest_start = true;
    prune(clean_start);
    pending.push_back(Pending{clean_start, clean_end});
  }

  bool next_chunk() {
    const char *start;
    const char *finish;
    if (!haystack->chunk(&start, &finish))
      return false;
    if (start == nullptr || finish == nullptr || start >= finish) {
      fail("Haystack returned an invalid chunk");
      return false;
    }
    current = start;
    end = finish;
    return true;
  }

  void close_line(addr q) {
    lines.push_back(Line{line_p, q, line_number});
    if (line_limit != 0 && lines.size() > line_limit)
      lines.pop_front();
  }

  void flush() {
    for (const Pending &match : pending) {
      LineCgrep::Match answer{match.p, match.q, 0, 0, 0, 0, 0, 0, false};
      auto first =
          std::find_if(lines.begin(), lines.end(), [&](const Line &line) {
            return line.p <= match.p && match.p <= line.q;
          });
      auto last =
          std::find_if(lines.begin(), lines.end(), [&](const Line &line) {
            return line.p <= match.q && match.q <= line.q;
          });
      if (first != lines.end() && last != lines.end()) {
        answer.lines_p = first->p;
        answer.lines_q = last->q;
        answer.start_line = first->number;
        answer.start_position =
            static_cast<std::size_t>(match.p - first->p) + 1;
        answer.end_line = last->number;
        answer.end_position = static_cast<std::size_t>(match.q - last->p) + 1;
        answer.has_lines = true;
      }
      ready.push_back(answer);
    }
    pending.clear();
  }

  void reclaim() {
    if (!active.empty() || !pending.empty() || !ready.empty())
      return;
    lines.clear();
    haystack->limit(line_p - 1);
  }

  bool next(LineCgrep::Match *answer) {
    if (answer == nullptr) {
      fail("LineCgrep::match got a null pointer");
      return false;
    }
    if (!error.empty())
      return false;
    if (!ready.empty()) {
      *answer = ready.front();
      ready.pop_front();
      return true;
    }
    reclaim();
    if (ended)
      return false;

    addr accepted_start;
    if (!started) {
      started = true;
      if (consume(static_cast<symbol>(special_symbol::START), -1,
                  &accepted_start))
        candidate(accepted_start, -1);
    }

    for (;;) {
      if (current != end) {
        addr here = offset++;
        symbol value = static_cast<unsigned char>(*current++);
        if (consume(value, here, &accepted_start))
          candidate(accepted_start, here);
        if (value == '\n') {
          close_line(here);
          line_p = here + 1;
          line_number++;
          flush();
        }
        if (!ready.empty()) {
          *answer = ready.front();
          ready.pop_front();
          return true;
        }
        reclaim();
        continue;
      }

      if (next_chunk())
        continue;
      if (!error.empty())
        return false;
      std::string cause;
      if (!haystack->success(&cause)) {
        fail(cause);
        return false;
      }

      ended = true;
      if (consume(static_cast<symbol>(special_symbol::END), offset,
                  &accepted_start))
        candidate(accepted_start, offset);
      if (line_p < offset)
        close_line(offset - 1);
      flush();
      if (!ready.empty()) {
        *answer = ready.front();
        ready.pop_front();
        return true;
      }
      reclaim();
      return false;
    }
  }

  bool translate(const LineCgrep::Match &match, const char **start,
                 const char **finish) {
    if (!error.empty())
      return false;
    if (!match.has_lines) {
      fail("LineCgrep match has no retained line text");
      return false;
    }
    if (haystack->translate(match.lines_p, match.lines_q, start, finish))
      return true;
    std::string cause;
    if (!haystack->success(&cause))
      fail(cause);
    else
      fail("Haystack could not translate line interval");
    return false;
  }

  bool reset(std::string *message) {
    if (!haystack->reset(message))
      return false;
    error.clear();
    initialize();
    return true;
  }

  bool success(std::string *message) {
    if (!error.empty()) {
      safe_error(message) = error;
      return false;
    }
    return haystack->success(message);
  }

  void fail(const std::string &message) {
    if (error.empty())
      error = message;
  }

  std::shared_ptr<const Cgrep::Machine> machine;
  std::shared_ptr<Haystack> haystack;
  std::size_t line_limit;
  std::vector<addr> starts;
  std::vector<addr> next_starts;
  std::vector<state> active;
  std::vector<state> next_active;
  std::deque<Line> lines;
  std::deque<Pending> pending;
  std::deque<LineCgrep::Match> ready;
  const char *current = nullptr;
  const char *end = nullptr;
  addr offset = 0;
  addr line_p = 0;
  addr largest_start = 0;
  std::size_t line_number = 1;
  std::string error;
  bool started = false;
  bool ended = false;
  bool have_largest_start = false;
};

std::shared_ptr<LineCgrep>
LineCgrep::make(std::shared_ptr<const Cgrep::Machine> machine,
                std::shared_ptr<Haystack> haystack, std::size_t lines,
                std::string *error) {
  if (machine == nullptr) {
    safe_error(error) = "LineCgrep needs a compiled machine";
    return nullptr;
  }
  if (haystack == nullptr) {
    safe_error(error) = "LineCgrep needs a Haystack";
    return nullptr;
  }
  return std::shared_ptr<LineCgrep>(new LineCgrep(
      std::make_unique<Impl>(std::move(machine), std::move(haystack), lines)));
}

std::shared_ptr<LineCgrep> LineCgrep::make(const std::string &expression,
                                           std::shared_ptr<Haystack> haystack,
                                           std::size_t lines,
                                           std::string *error) {
  std::shared_ptr<const Cgrep::Machine> machine =
      Cgrep::compile(expression, error);
  if (machine == nullptr)
    return nullptr;
  return make(std::move(machine), std::move(haystack), lines, error);
}

std::shared_ptr<LineCgrep>
LineCgrep::make(const std::vector<transition> &transitions,
                std::shared_ptr<Haystack> haystack, std::size_t lines,
                std::string *error) {
  std::shared_ptr<const Cgrep::Machine> machine =
      Cgrep::compile(transitions, error);
  if (machine == nullptr)
    return nullptr;
  return make(std::move(machine), std::move(haystack), lines, error);
}

LineCgrep::LineCgrep(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

LineCgrep::~LineCgrep() = default;

bool LineCgrep::match(Match *match) { return impl_->next(match); }

std::string LineCgrep::translate(const Match &match) {
  const char *start;
  const char *end;
  if (!translate(match, &start, &end))
    return "";
  return std::string(start, end);
}

bool LineCgrep::translate(const Match &match, const char **start,
                          const char **end) {
  return impl_->translate(match, start, end);
}

bool LineCgrep::reset(std::string *error) { return impl_->reset(error); }

bool LineCgrep::success(std::string *error) { return impl_->success(error); }

} // namespace regexp
} // namespace cottontail
