#ifndef COTTONTAIL_REGEXP_HAYSTACK_CGREP_H_
#define COTTONTAIL_REGEXP_HAYSTACK_CGREP_H_

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
  friend class LineCgrep;
  struct Machine;
  static std::shared_ptr<const Machine>
  machine(std::shared_ptr<const Cgrep::Machine> bundle);

  HaystackCgrep(std::shared_ptr<const Machine> machine,
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

  std::shared_ptr<const Machine> machine_;
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
} // namespace regexp
} // namespace cottontail

#endif // COTTONTAIL_REGEXP_HAYSTACK_CGREP_H_
