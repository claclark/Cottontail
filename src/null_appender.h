#ifndef COTTONTAIL_SRC_NULL_APPENDER_H_
#define COTTONTAIL_SRC_NULL_APPENDER_H_

#include <memory>
#include <string>

#include "src/annotator.h"
#include "src/appender.h"
#include "src/committable.h"
#include "src/core.h"
#include "src/featurizer.h"
#include "src/tokenizer.h"
#include "src/working.h"

namespace cottontail {

class NullAppender : public Appender {
public:
  static std::shared_ptr<Appender> make(const std::string &recipe,
                                        std::string *error = nullptr) {
    if (recipe == "") {
      return std::shared_ptr<Appender>(new NullAppender());
    } else {
      safe_set(error) = "NullAppender can't make recipe: " + recipe;
      return nullptr;
    }
  }
  static bool check(const std::string &recipe, std::string *error = nullptr) {
    if (recipe == "") {
      return true;
    } else {
      safe_set(error) = "NullAppender can't make recipe: " + recipe;
      return false;
    }
  }

  virtual ~NullAppender(){};
  NullAppender(const NullAppender &) = delete;
  NullAppender &operator=(const NullAppender &) = delete;
  NullAppender(NullAppender &&) = delete;
  NullAppender &operator=(NullAppender &&) = delete;

private:
  NullAppender(){};
  std::string recipe_() final { return ""; }
  bool append_(const std::string &text, addr *p, addr *q,
               std::string *error) final {
    return true;
  }
  bool transaction_(std::string *error) final { return true; }
};

} // namespace cottontail

#endif // COTTONTAIL_SRC_NULL_APPENDER_H_
