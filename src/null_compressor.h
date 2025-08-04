#ifndef COTTONTAIL_SRC_NULL_COMPRESSOR_H_
#define COTTONTAIL_SRC_NULL_COMPRESSOR_H_

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>

#include "src/compressor.h"

namespace cottontail {

class NullCompressor : public Compressor {
public:
  NullCompressor(){};
  static std::shared_ptr<Compressor> make(const std::string &recipe,
                                          std::string *error = nullptr) {
    return make();
  }
  static std::shared_ptr<Compressor> make() {
    return std::make_shared<NullCompressor>();
  }
  static bool check(const std::string &recipe, std::string *error = nullptr) {
    if (recipe == "") {
      return true;
    } else {
      safe_error(error) = "Bad NullCompressor recipe";
      return false;
    }
  }

  virtual ~NullCompressor(){};
  NullCompressor(const NullCompressor &) = delete;
  NullCompressor &operator=(const NullCompressor &) = delete;
  NullCompressor(NullCompressor &&) = delete;
  NullCompressor &operator=(NullCompressor &&) = delete;

private:
  size_t crush_(char *in, size_t length, char *out, size_t available) final {
    memcpy(out, in, std::min(length, available));
    return std::min(length, available);
  };
  size_t tang_(char *in, size_t length, char *out, size_t available) final {
    memcpy(out, in, std::min(length, available));
    return std::min(length, available);
  };
  bool destructive_() final { return false; }
  addr extra_(addr n) final { return 0; }
};

} // namespace cottontail
#endif // COTTONTAIL_SRC_NULL_COMPRESSOR_H_
