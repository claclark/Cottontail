#ifndef COTTONTAIL_SRC_BAD_COMPRESSOR_H_
#define COTTONTAIL_SRC_BAD_COMPRESSOR_H_

// A very bad compressor used for testing.

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>

#include "src/compressor.h"

namespace cottontail {

class BadCompressor : public Compressor {
public:
  BadCompressor(){};
  static std::shared_ptr<Compressor> make(const std::string &recipe,
                                          std::string *error = nullptr) {
    return make();
  }
  static std::shared_ptr<Compressor> make() {
    return std::make_shared<BadCompressor>();
  }
  static bool check(const std::string &recipe, std::string *error = nullptr) {
    return true;
  }

private:
  size_t crush_(char *in, size_t length, char *out, size_t available) final {
    assert(available >= 2 * length);
    for (size_t i = 0; i < length; i++) {
      *out++ = *in;
      *out++ = *in++;
    }
    return 2 * length;
  };
  size_t tang_(char *in, size_t length, char *out, size_t available) final {
    assert(length % 2 == 0 && available >= length / 2);
    for (size_t i = 0; i < length; i += 2) {
      assert(*in == *(in + 1));
      *out++ = *in;
      in += 2;
    }
    return length / 2;
  };
  bool destructive_() final { return false; }
  addr extra_(addr n) final { return n; }
};

} // namespace cottontail
#endif // COTTONTAIL_SRC_BAD_COMPRESSOR_H_
