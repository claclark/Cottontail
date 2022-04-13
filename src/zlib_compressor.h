#ifndef COTTONTAIL_SRC_ZLIB_COMPRESSOR_H_
#define COTTONTAIL_SRC_ZLIB_COMPRESSOR_H_

#include <cstdint>
#include <memory>

#include "src/compressor.h"

namespace cottontail {

// ZlibCompressor will NOT trash it's input

class ZlibCompressor : public Compressor {
public:
  ZlibCompressor(){};
  static std::shared_ptr<Compressor> make(const std::string &recipe,
                                          std::string *error = nullptr) {
    return make();
  }
  static std::shared_ptr<Compressor> make() {
    return std::make_shared<ZlibCompressor>();
  }
  static bool check(const std::string &recipe, std::string *error = nullptr) {
    if (recipe == "") {
      return true;
    } else {
      safe_set(error) = "Bad ZlibCompressor recipe";
      return false;
    }
  }

private:
  size_t crush_(char *in, size_t length, char *out, size_t available) final;
  size_t tang_(char *in, size_t length, char *out, size_t available) final;
  bool destructive_() final { return false; };
  addr extra_(addr n) final { return 64 * 1024; };
};

} // namespace cottontail
#endif // COTTONTAIL_SRC_ZLIB_COMPRESSOR_H_
