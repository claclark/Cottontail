#ifndef COTTONTAIL_REGEXP_CGREP_H_
#define COTTONTAIL_REGEXP_CGREP_H_

#include <istream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "regexp/haystack.h"
#include "regexp/nfa.h"
#include "src/core.h"

namespace cottontail {
namespace regexp {

// Source-independent shortest-substring matching.
class Cgrep {
public:
  struct Machine;

  // Validate the NFA once. Runners lazily compile their immutable machines.
  static std::shared_ptr<const Machine> compile(const std::string &expression,
                                                std::string *error = nullptr,
                                                bool springy = true);
  static std::shared_ptr<const Machine>
  compile(const std::vector<transition> &nfa, std::string *error = nullptr,
          bool springy = true);

  static std::shared_ptr<Cgrep> make(std::shared_ptr<const Machine> machine,
                                     std::shared_ptr<Haystack> haystack,
                                     std::string *error = nullptr);
  static std::shared_ptr<Cgrep> make(std::shared_ptr<const Machine> machine,
                                     const std::string &filename,
                                     std::string *error = nullptr);
  static std::shared_ptr<Cgrep> make(std::shared_ptr<const Machine> machine,
                                     std::shared_ptr<std::istream> input,
                                     std::string *error = nullptr);

  // Shared storage is retained for the matcher's lifetime and must not change.
  static std::shared_ptr<Cgrep> make(std::shared_ptr<const Machine> machine,
                                     std::shared_ptr<const char> buffer,
                                     std::size_t size,
                                     std::string *error = nullptr);
  // An unowned byte range is copied into storage owned by the matcher.
  static std::shared_ptr<Cgrep> make(std::shared_ptr<const Machine> machine,
                                     const char *buffer, std::size_t size,
                                     std::string *error = nullptr);

  static std::shared_ptr<Cgrep> make(const std::string &expression,
                                     std::shared_ptr<Haystack> haystack,
                                     std::string *error = nullptr);
  static std::shared_ptr<Cgrep> make(const std::vector<transition> &nfa,
                                     std::shared_ptr<Haystack> haystack,
                                     std::string *error = nullptr);

  // Return the next zero-based inclusive byte interval.
  bool match(addr *p, addr *q) { return match_(p, q); }
  std::string translate(addr p, addr q);
  // A half-open view, valid until the next match, pointer translation, or
  // reset.
  bool translate(addr p, addr q, const char **start, const char **end) {
    return translate_(p, q, start, end);
  }
  bool reset(std::string *error = nullptr) { return reset_(error); }
  bool success(std::string *error = nullptr) { return success_(error); }

  virtual ~Cgrep() {}
  Cgrep(const Cgrep &) = delete;
  Cgrep &operator=(const Cgrep &) = delete;
  Cgrep(Cgrep &&) = delete;
  Cgrep &operator=(Cgrep &&) = delete;

protected:
  explicit Cgrep(std::shared_ptr<Haystack> haystack)
      : haystack_(std::move(haystack)) {}
  Cgrep(std::shared_ptr<const char> buffer, std::size_t size)
      : buffer_(std::move(buffer)), size_(size) {}

  std::shared_ptr<Haystack> haystack_;
  std::shared_ptr<const char> buffer_;
  std::size_t size_ = 0;

private:
  friend class LineCgrep;
  virtual bool match_(addr *p, addr *q) = 0;
  virtual bool translate_(addr p, addr q, const char **start,
                          const char **end) = 0;
  virtual bool reset_(std::string *error) = 0;
  virtual bool success_(std::string *error) = 0;
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
  static std::shared_ptr<LineCgrep>
  make(std::shared_ptr<const Cgrep::Machine> machine,
       const std::string &filename, std::size_t lines,
       std::string *error = nullptr);
  static std::shared_ptr<LineCgrep>
  make(std::shared_ptr<const Cgrep::Machine> machine,
       std::shared_ptr<std::istream> input, std::size_t lines,
       std::string *error = nullptr);
  static std::shared_ptr<LineCgrep>
  make(std::shared_ptr<const Cgrep::Machine> machine,
       std::shared_ptr<const char> buffer, std::size_t size, std::size_t lines,
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
  struct HaystackImpl;
  struct BufferImpl;

  static std::shared_ptr<LineCgrep>
  from_raw(std::shared_ptr<const Cgrep::Machine> machine,
           std::shared_ptr<Cgrep> raw, std::size_t lines, std::string *error);
  static std::shared_ptr<LineCgrep> from_buffer(std::shared_ptr<Cgrep> raw,
                                                std::size_t lines);

  explicit LineCgrep(std::unique_ptr<Impl> impl);

  std::unique_ptr<Impl> impl_;
};

} // namespace regexp
} // namespace cottontail

#endif // COTTONTAIL_REGEXP_CGREP_H_
