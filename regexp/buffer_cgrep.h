#ifndef COTTONTAIL_REGEXP_BUFFER_CGREP_H_
#define COTTONTAIL_REGEXP_BUFFER_CGREP_H_

#include "regexp/cgrep.h"

namespace cottontail {
namespace regexp {

class BufferCgrep final : public Cgrep {
public:
  static std::shared_ptr<Cgrep>
  make(std::shared_ptr<const Cgrep::Machine> machine,
       std::shared_ptr<const char> buffer, std::size_t size,
       std::string *error = nullptr);
  virtual ~BufferCgrep() {}

private:
  struct Machine;
  static std::shared_ptr<const Machine>
  machine(std::shared_ptr<const Cgrep::Machine> bundle);

  BufferCgrep(std::shared_ptr<const Machine> machine,
              std::shared_ptr<const char> buffer, std::size_t size);
  bool match_(addr *p, addr *q) final;
  bool translate_(addr p, addr q, const char **start, const char **end) final;
  bool reset_(std::string *error) final;
  bool success_(std::string *error) final;
  void fail(const std::string &message);

  std::shared_ptr<const Machine> machine_;
  const char *current_;
  const char *end_;
  std::string error_;
};
} // namespace regexp
} // namespace cottontail

#endif // COTTONTAIL_REGEXP_BUFFER_CGREP_H_
