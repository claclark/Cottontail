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
  BufferCgrep(std::shared_ptr<const BufferMachine> machine,
              std::shared_ptr<const char> buffer, std::size_t size);
  bool match_(addr *p, addr *q) final;
  bool translate_(addr p, addr q, const char **start, const char **end) final;
  bool reset_(std::string *error) final;
  bool success_(std::string *error) final;
  void fail(const std::string &message);

  std::shared_ptr<const BufferMachine> machine_;
  const char *current_;
  const char *end_;
  std::string error_;
};

class BufferLineCgrep final : public LineCgrep {
public:
  static std::shared_ptr<LineCgrep> make(std::shared_ptr<Cgrep> raw,
                                         std::size_t lines,
                                         std::string *error = nullptr);
  virtual ~BufferLineCgrep() {}

private:
  struct Line {
    addr p = 0;
    addr q = -1;
    std::size_t number = 1;
  };
  BufferLineCgrep(std::shared_ptr<Cgrep> raw, std::size_t limit);
  void advance(Line *line, addr position);
  bool match_(LineCgrep::Match *answer) final;
  bool translate_(const LineCgrep::Match &match, const char **start,
                  const char **end) final;
  bool reset_(std::string *message) final;
  bool success_(std::string *message) final;
  void fail(const std::string &message);
  std::shared_ptr<Cgrep> raw;
  std::size_t line_limit;
  Line first;
  Line last;
  std::string error;
};

} // namespace regexp
} // namespace cottontail

#endif // COTTONTAIL_REGEXP_BUFFER_CGREP_H_
