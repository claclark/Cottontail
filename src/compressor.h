#ifndef COTTONTAIL_SRC_COMPRESSOR_H_
#define COTTONTAIL_SRC_COMPRESSOR_H_

#include <cassert>
#include <cstdint>
#include <memory>
#include <string>

#include "src/core.h"

namespace cottontail {

// Compressors are potentially allowed to trash their input. Check destructive()
// if it matters.  To some extent, compressors are allowed expand the size of
// the data.  Leave extra(n) space available in the output buffer for an input
// of size n.

class Compressor {
public:
  static std::shared_ptr<Compressor> make(const std::string &name,
                                          const std::string &recipe,
                                          std::string *error = nullptr);
  static bool check(const std::string &name, const std::string &recipe,
                    std::string *error = nullptr);
  size_t crush(char *in, size_t length, char *out, size_t available) {
    // could die mysteriously elsewhere if not true
    assert(available >= length + extra(length));
    if (length == 0)
      return 0;
    else
      return crush_(in, length, out, available);
  };
  size_t tang(char *in, size_t length, char *out, size_t available) {
    if (length == 0)
      return 0;
    else
      return tang_(in, length, out, available);
  };
  bool destructive() { return destructive_(); };
  addr extra(addr n) { return extra_(n); };

  virtual ~Compressor(){};
  Compressor(const Compressor &) = delete;
  Compressor &operator=(const Compressor &) = delete;
  Compressor(Compressor &&) = delete;
  Compressor &operator=(Compressor &&) = delete;

protected:
  Compressor(){};

private:
  virtual size_t crush_(char *in, size_t length, char *out,
                        size_t available) = 0;
  virtual size_t tang_(char *in, size_t length, char *out,
                       size_t available) = 0;
  virtual bool destructive_() = 0;
  virtual addr extra_(addr n) = 0;
};
} // namespace cottontail
#endif //  COTTONTAIL_SRC_COMPRESSOR_H_
