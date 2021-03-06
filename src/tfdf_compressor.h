#ifndef COTTONTAIL_SRC_TFDF_COMPRESSOR_H_
#define COTTONTAIL_SRC_TFDF_COMPRESSOR_H_

// Standard vbyte compression

#include <cstdint>
#include <memory>

#include "src/compressor.h"

namespace cottontail {

class TfdfCompressor : public Compressor {
public:
  TfdfCompressor(){};
  static std::shared_ptr<Compressor> make(const std::string &recipe,
                                          std::string *error = nullptr) {
    std::shared_ptr<TfdfCompressor> compressor =
        std::make_shared<TfdfCompressor>();
    if (recipe == "component")
      compressor->component_ = true;
    else if (recipe != "")
      return nullptr;
    return compressor;
  }
  static std::shared_ptr<Compressor> make() {
    return std::make_shared<TfdfCompressor>();
  }
  static bool check(const std::string &recipe, std::string *error = nullptr) {
    if (recipe == "" || recipe == "component") {
      return true;
    } else {
      safe_set(error) = "Bad TfdfCompressor recipe";
      return false;
    }
  }

  virtual ~TfdfCompressor(){};
  TfdfCompressor(const TfdfCompressor &) = delete;
  TfdfCompressor &operator=(const TfdfCompressor &) = delete;
  TfdfCompressor(TfdfCompressor &&) = delete;
  TfdfCompressor &operator=(TfdfCompressor &&) = delete;

private:
  bool component_ = false;
  size_t crush_(char *in, size_t length, char *out, size_t available) final;
  size_t tang_(char *in, size_t length, char *out, size_t available) final;
  bool destructive_() final { return false; };
  addr extra_(addr n) final { return n + 1; };
};

} // namespace cottontail
#endif // COTTONTAIL_SRC_TFDF_COMPRESSOR_H_
