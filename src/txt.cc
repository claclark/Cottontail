#include "src/txt.h"

#include <memory>
#include <string>

#include "src/core.h"
#include "src/json_txt.h"
#include "src/simple_txt.h"

namespace cottontail {

std::shared_ptr<Txt> Txt::make(const std::string &name,
                               const std::string &recipe, std::string *error,
                               std::shared_ptr<Tokenizer> tokenizer,
                               std::shared_ptr<Working> working) {
  std::shared_ptr<Txt> txt;
  if (name == "" || name == "simple") {
    txt = SimpleTxt::make(recipe, tokenizer, working, error);
  } else {
    safe_set(error) = "No Txt named: " + name;
    txt = nullptr;
  }
  if (txt != nullptr)
    return wrap(recipe, txt, error);
  else
    return nullptr;
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

std::shared_ptr<Txt> Txt::wrap(const std::string &recipe,
                               std::shared_ptr<Txt> txt, std::string *error) {
  return JsonTxt::wrap(recipe, txt, error);
}

std::shared_ptr<Txt> Txt::clone_(std::string *error) {
  safe_set(error) = "Txt type does not support cloning: " + name();
  return nullptr;
}
} // namespace cottontail
