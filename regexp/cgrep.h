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
  std::string error_;
  bool started_ = false;
  bool ended_ = false;
  bool have_largest_start_ = false;
  bool have_pending_limit_ = false;
};

} // namespace regexp
} // namespace cottontail

#endif // COTTONTAIL_REGEXP_CGREP_H_
