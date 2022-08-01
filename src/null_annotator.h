#ifndef COTTONTAIL_SRC_NULL_ANNOTATOR_H_
#define COTTONTAIL_SRC_NULL_ANNOTATOR_H_

#include <memory>
#include <string>

#include "src/annotator.h"
#include "src/core.h"
#include "src/working.h"

namespace cottontail {

class NullAnnotator : public Annotator {
public:
  static std::shared_ptr<Annotator> make(const std::string &recipe,
                                         std::string *error = nullptr) {
    if (recipe == "") {
      return std::shared_ptr<Annotator>(new NullAnnotator());
    } else {
      safe_set(error) = "NullAnnotator can't make recipe: " + recipe;
      return nullptr;
    }
  };
  static bool check(const std::string &recipe, std::string *error = nullptr) {
    if (recipe == "") {
      return true;
    } else {
      safe_set(error) = "NullAnnotator can't make recipe: " + recipe;
      return false;
    }
  };

  virtual ~NullAnnotator(){};
  NullAnnotator(const NullAnnotator &) = delete;
  NullAnnotator &operator=(const NullAnnotator &) = delete;
  NullAnnotator(NullAnnotator &&) = delete;
  NullAnnotator &operator=(NullAnnotator &&) = delete;

private:
  NullAnnotator(){};
  std::string recipe_() final { return ""; }
  bool annotate_(addr feature, addr p, addr q, fval v,
                 std::string *error) final {
    return true;
  }
  bool transaction_(std::string *error) final { return true; }
};

} // namespace cottontail

#endif // COTTONTAIL_SRC_NULL_ANNOTATOR_H_
