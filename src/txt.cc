#include "src/txt.h"

#include <memory>
#include <string>

#include "src/core.h"
#include "src/simple_txt.h"

namespace cottontail {

std::shared_ptr<Txt> Txt::make(const std::string &name,
                               const std::string &recipe, std::string *error,
                               std::shared_ptr<Tokenizer> tokenizer,
                               std::shared_ptr<Working> working) {
  std::shared_ptr<Txt> txt;
  if (name == "" || name == "simple") {
    txt = SimpleTxt::make(recipe, tokenizer, working, error);
    if (txt != nullptr)
      txt->name_ = "simple";
    return txt;
  } else {
    safe_set(error) = "No Txt named: " + name;
    return nullptr;
  }
}

bool Txt::check(const std::string &name, const std::string &recipe,
                std::string *error) {
  if (name == "" || name == "simple") {
    return SimpleTxt::check(recipe, error);
  } else {
    safe_set(error) = "No Txt named: " + name;
    return false;
  }
}
} // namespace cottontail
