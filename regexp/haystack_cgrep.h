#ifndef COTTONTAIL_REGEXP_HAYSTACK_CGREP_H_
#define COTTONTAIL_REGEXP_HAYSTACK_CGREP_H_

#include <deque>

#include "regexp/cgrep.h"

namespace cottontail {
namespace regexp {

class HaystackCgrep final : public Cgrep {
public:
  static std::shared_ptr<Cgrep>
  make(std::shared_ptr<const Cgrep::Machine> machine,
       std::shared_ptr<Haystack> haystack, std::string *error = nullptr);
  virtual ~HaystackCgrep() {}

private:
  HaystackCgrep(std::shared_ptr<const HaystackMachine> machine,
                std::shared_ptr<Haystack> haystack);
  bool match_(addr *p, addr *q) final;
  bool translate_(addr p, addr q, const char **start, const char **end) final;
  bool reset_(std::string *error) final;
  bool success_(std::string *error) final;

  bool consume(symbol value, addr end, addr *accepted_start);
  bool candidate(addr start, addr end, addr *p, addr *q);
  bool next_chunk();
  void advance_limit(addr x);
  void fail(const std::string &message);
  void initialize();
  void prune(addr p);

  std::shared_ptr<const HaystackMachine> machine_;
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

class HaystackLineCgrep final : public LineCgrep {
public:
  static std::shared_ptr<LineCgrep>
  make(std::shared_ptr<const Cgrep::Machine> machine,
       std::shared_ptr<Haystack> haystack, std::size_t lines,
       std::string *error = nullptr);
  virtual ~HaystackLineCgrep() {}

private:
  struct Line {
    addr p;
    addr q;
    std::size_t number;
  };
  struct Pending {
    addr p;
    addr q;
  };
  HaystackLineCgrep(std::shared_ptr<const HaystackMachine> machine,
                    std::shared_ptr<Haystack> haystack, std::size_t limit);
  void initialize();
  bool consume(symbol value, addr finish, addr *accepted_start);
  void prune(addr p);
  void candidate(addr start, addr finish);
  bool next_chunk();
  void close_line(addr q);
  void flush();
  void reclaim();
  bool match_(LineCgrep::Match *answer) final;
  bool translate_(const LineCgrep::Match &match, const char **start,
                  const char **finish) final;
  bool reset_(std::string *message) final;
  bool success_(std::string *message) final;
  void fail(const std::string &message);
  std::shared_ptr<const HaystackMachine> machine;
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
  addr limited_through = -1;
  std::size_t line_number = 1;
  std::string error;
  bool started = false;
  bool ended = false;
  bool have_largest_start = false;
};

} // namespace regexp
} // namespace cottontail

#endif // COTTONTAIL_REGEXP_HAYSTACK_CGREP_H_
