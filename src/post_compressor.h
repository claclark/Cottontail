#ifndef COTTONTAIL_SRC_POST_COMPRESSOR_H_
#define COTTONTAIL_SRC_POST_COMPRESSOR_H_

// Standard vbyte compression

#include <cstdint>
#include <memory>

#include "src/compressor.h"

namespace cottontail {

class PostCompressor : public Compressor {
public:
  PostCompressor(){};
  static std::shared_ptr<Compressor> make(const std::string &recipe,
                                          std::string *error = nullptr) {
    return make();
  }
  static std::shared_ptr<Compressor> make() {
    return std::make_shared<PostCompressor>();
  }
  static bool check(const std::string &recipe, std::string *error = nullptr) {
    if (recipe == "") {
      return true;
    } else {
      safe_set(error) = "Bad Postcompressor recipe";
      return false;
    }
  }

  virtual ~PostCompressor(){};
  PostCompressor(const PostCompressor &) = delete;
  PostCompressor &operator=(const PostCompressor &) = delete;
  PostCompressor(PostCompressor &&) = delete;
  PostCompressor &operator=(PostCompressor &&) = delete;

private:
  size_t crush_(char *in, size_t length, char *out, size_t available) final;
  size_t tang_(char *in, size_t length, char *out, size_t available) final;
  bool destructive_() final { return false; };
  addr extra_(addr n) final { return n / sizeof(addr) + 2; };
};

} // namespace cottontail
#endif // COTTONTAIL_SRC_POST_COMPRESSOR_H_
