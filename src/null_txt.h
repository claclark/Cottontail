#ifndef COTTONTAIL_SRC_NULL_TXT_H_
#define COTTONTAIL_SRC_NULL_TXT_H_

#include <memory>
#include <string>
#include <vector>

#include "src/core.h"
#include "src/txt.h"

namespace cottontail {

class NullTxt final : public Txt {
public:
  static std::shared_ptr<Txt> make(const std::string &recipe,
                                   std::string *error = nullptr) {
    if (recipe == "") {
      return std::shared_ptr<Txt>(new NullTxt());
    } else {
      safe_set(error) = "NullTxt can't make recipe: " + recipe;
      return nullptr;
    }
  };
  static bool check(const std::string &recipe, std::string *error = nullptr) {
    if (recipe == "") {
      return true;
    } else {
      safe_set(error) = "NullTxt can't make recipe: " + recipe;
      return false;
    }
  };

  virtual ~NullTxt(){};
  NullTxt(const NullTxt &) = delete;
  NullTxt &operator=(const NullTxt &) = delete;
  NullTxt(NullTxt &&) = delete;
  NullTxt &operator=(NullTxt &&) = delete;

private:
  NullTxt() {};
  std::string recipe_() { return ""; };
  std::string translate_(addr p, addr q) { return ""; }
  addr tokens_() { return 0; };
};

} // namespace cottontail
#endif // COTTONTAIL_SRC_NULL_TXT_H_
