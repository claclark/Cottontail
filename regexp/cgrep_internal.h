#ifndef COTTONTAIL_REGEXP_CGREP_INTERNAL_H_
#define COTTONTAIL_REGEXP_CGREP_INTERNAL_H_

#include <mutex>

#include "regexp/cgrep.h"

namespace cottontail {
namespace regexp {

// Only the cache slots are mutable. Each runner holds its own immutable
// compiled representation after releasing this mutex.
struct Cgrep::Machine final {
  std::vector<transition> transitions;
  std::size_t state_count;
  bool springy;
  mutable std::mutex mutex;
  mutable std::shared_ptr<const void> haystack;
  mutable std::shared_ptr<const void> buffer;
};

struct LineCgrep::Impl {
  virtual ~Impl() {}
  virtual bool next(LineCgrep::Match *answer) = 0;
  virtual bool translate(const LineCgrep::Match &match, const char **start,
                         const char **end) = 0;
  virtual bool reset(std::string *error) = 0;
  virtual bool success(std::string *error) = 0;
};

} // namespace regexp
} // namespace cottontail

#endif // COTTONTAIL_REGEXP_CGREP_INTERNAL_H_
