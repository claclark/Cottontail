#ifndef COTTONTAIL_REGEXP_CGREP_H_
#define COTTONTAIL_REGEXP_CGREP_H_

#include <memory>
#include <string>
#include <vector>

#include "regexp/haystack.h"
#include "regexp/nfa.h"
#include "src/core.h"

namespace cottontail {
namespace regexp {

class LineCgrep;

// Stateful shortest-substring matching over a Haystack.
class Cgrep {
public:
  struct Machine;

  // Compile once and share the immutable machine between runners.
  static std::shared_ptr<const Machine> compile(const std::string &expression,
                                                std::string *error = nullptr);
  static std::shared_ptr<const Machine>
  compile(const std::vector<transition> &nfa, std::string *error = nullptr);

  static std::shared_ptr<Cgrep> make(std::shared_ptr<const Machine> machine,
                                     std::shared_ptr<Haystack> haystack,
                                     std::string *error = nullptr);
  static std::shared_ptr<Cgrep> make(const std::string &expression,
                                     std::shared_ptr<Haystack> haystack,
                                     std::string *error = nullptr);
  static std::shared_ptr<Cgrep> make(const std::vector<transition> &nfa,
                                     std::shared_ptr<Haystack> haystack,
                                     std::string *error = nullptr);

  // Return the next zero-based inclusive byte interval.
  bool match(addr *p, addr *q);

  std::string translate(addr p, addr q);
  bool translate(addr p, addr q, const char **start, const char **end);

  bool reset(std::string *error = nullptr);
  bool success(std::string *error = nullptr);

  Cgrep(const Cgrep &) = delete;
  Cgrep &operator=(const Cgrep &) = delete;
  Cgrep(Cgrep &&) = delete;
  Cgrep &operator=(Cgrep &&) = delete;

private:
  Cgrep(std::shared_ptr<const Machine> machine,
        std::shared_ptr<Haystack> haystack);

  bool consume(symbol value, addr end, addr *accepted_start);
  bool candidate(addr start, addr end, addr *p, addr *q);
  bool next_chunk();
  void advance_limit(addr x);
  void fail(const std::string &message);
  void initialize();
  void prune(addr p);

  std::shared_ptr<const Machine> machine_;
  std::shared_ptr<Haystack> haystack_;
  std::vector<addr> starts_;
  std::vector<addr> next_starts_;
  std::vector<state> active_;
  std::vector<state> next_active_;
  const char *current_ = nullptr;
  const char *end_ = nullptr;
  addr offset_ = 0;
  addr largest_start_ = 0;
  addr pending_limit_ = 0;
  addr limited_through_ = -1;
  std::string error_;
  bool started_ = false;
  bool ended_ = false;
  bool have_largest_start_ = false;
  bool have_pending_limit_ = false;
};

// Shortest-substring matching reported as complete lines. The raw match is
// always present; line coordinates and text are available when the match fits
// in the configured line window.
class LineCgrep {
public:
  struct Match {
    addr p;
    addr q;
    addr lines_p;
    addr lines_q;
    std::size_t start_line;
    std::size_t start_position;
    std::size_t end_line;
    std::size_t end_position;
    bool has_lines;
  };

  static std::shared_ptr<LineCgrep>
  make(std::shared_ptr<const Cgrep::Machine> machine,
       std::shared_ptr<Haystack> haystack, std::size_t lines,
       std::string *error = nullptr);
  static std::shared_ptr<LineCgrep> make(const std::string &expression,
                                         std::shared_ptr<Haystack> haystack,
                                         std::size_t lines,
                                         std::string *error = nullptr);
  static std::shared_ptr<LineCgrep> make(const std::vector<transition> &nfa,
                                         std::shared_ptr<Haystack> haystack,
                                         std::size_t lines,
                                         std::string *error = nullptr);

  ~LineCgrep();

  bool match(Match *match);

  std::string translate(const Match &match);
  bool translate(const Match &match, const char **start, const char **end);

  bool reset(std::string *error = nullptr);
  bool success(std::string *error = nullptr);

  LineCgrep(const LineCgrep &) = delete;
  LineCgrep &operator=(const LineCgrep &) = delete;
  LineCgrep(LineCgrep &&) = delete;
  LineCgrep &operator=(LineCgrep &&) = delete;

private:
  struct Impl;

  explicit LineCgrep(std::unique_ptr<Impl> impl);

  std::unique_ptr<Impl> impl_;
};

} // namespace regexp
} // namespace cottontail

#endif // COTTONTAIL_REGEXP_CGREP_H_
