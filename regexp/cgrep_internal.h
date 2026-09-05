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

} // namespace regexp
} // namespace cottontail

#endif // COTTONTAIL_REGEXP_CGREP_INTERNAL_H_
