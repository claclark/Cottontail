#ifndef COTTONTAIL_REGEXP_HAYSTACK_H_
#define COTTONTAIL_REGEXP_HAYSTACK_H_

#include <memory>
#include <string>

#include "src/core.h"

namespace cottontail {
namespace regexp {

// A replayable or streaming source of bytes. Chunk boundaries have no
// matching semantics.
class Haystack {
public:
  static std::shared_ptr<Haystack> make(const std::string &filename,
                                        std::string *error = nullptr);
  static std::shared_ptr<Haystack> make_stdin(std::string *error = nullptr);

  virtual ~Haystack() = default;

  // Return the next nonempty half-open byte range. False means EOF or error;
  // success() distinguishes them.
  virtual bool chunk(const char **start, const char **end) = 0;

  // Translate an inclusive absolute byte interval. The first form owns its
  // result. Pointers from the second form are deliberately short-lived.
  virtual std::string translate(addr p, addr q) = 0;
  virtual bool translate(addr p, addr q, const char **start,
                         const char **end) = 0;

  // No future translation will begin at or before x.
  virtual void limit(addr x) = 0;

  virtual bool reset(std::string *error = nullptr) = 0;
  virtual bool success(std::string *error = nullptr) = 0;
};

} // namespace regexp
} // namespace cottontail

#endif // COTTONTAIL_REGEXP_HAYSTACK_H_
