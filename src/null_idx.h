#ifndef COTTONTAIL_SRC_NULL_IDX_H_
#define COTTONTAIL_SRC_NULL_IDX_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "src/compressor.h"
#include "src/core.h"
#include "src/hopper.h"
#include "src/idx.h"
#include "src/working.h"

namespace cottontail {

class NullIdx final : public Idx {
public:
  static std::shared_ptr<Idx> make(const std::string &recipe,
                                   std::string *error = nullptr) {
    if (recipe == "") {
      return std::shared_ptr<Idx>(new NullIdx());
    } else {
      safe_error(error) = "NullIdx can't make recipe: " + recipe;
      return nullptr;
    }
  };
  static bool check(const std::string &recipe, std::string *error = nullptr) {
    if (recipe == "") {
      return true;
    } else {
      safe_error(error) = "NullIdx can't make recipe: " + recipe;
      return false;
    }
  }
  static bool recover(const std::string &recipe, bool commit = false,
                      std::string *error = nullptr,
                      std::shared_ptr<Working> working = nullptr) {
    return true;
  };

  virtual ~NullIdx(){};
  NullIdx(const NullIdx &) = delete;
  NullIdx &operator=(const NullIdx &) = delete;
  NullIdx(NullIdx &&) = delete;
  NullIdx &operator=(NullIdx &&) = delete;

private:
  NullIdx(){};
  std::string recipe_() final { return ""; };
  std::unique_ptr<Hopper> hopper_(addr feature) final {
    return std::make_unique<EmptyHopper>();
  };
  addr count_(addr feature) final { return 0; };
  addr vocab_() final { return 0; };
  void reset_() final { return; }
};

} // namespace cottontail
#endif // COTTONTAIL_SRC_NULL_IDX_H_
