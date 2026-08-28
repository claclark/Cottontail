#include "src/tokenizer.h"

#include <memory>
#include <string>

#include "src/ascii_tokenizer.h"
#include "src/core.h"
#include "src/ngram_tokenizer.h"
#include "src/utf8_tokenizer.h"

namespace cottontail {

std::shared_ptr<Tokenizer> Tokenizer::make(const std::string &name,
                                           const std::string &recipe,
                                           std::string *error) {

  std::shared_ptr<Tokenizer> tokenizer = nullptr;
  if (name == "" || name == "ascii") {
    tokenizer = AsciiTokenizer::make(recipe, error);
  } else if (name == "utf8") {
    tokenizer = Utf8Tokenizer::make(recipe, error);
  } else if (name == "ngram") {
    tokenizer = NGramTokenizer::make(recipe, error);
  } else {
    safe_error(error) = "No Tokenizer named: " + name;
  }
  if (tokenizer != nullptr)
    tokenizer->name_ = name == "" ? "ascii" : name;
  return tokenizer;
}

bool Tokenizer::check(const std::string &name, const std::string &recipe,
                      std::string *error) {

  if (name == "" || name == "ascii") {
    return AsciiTokenizer::check(recipe, error);
  } else if (name == "utf8") {
    return Utf8Tokenizer::check(recipe, error);
  } else if (name == "ngram") {
    return NGramTokenizer::check(recipe, error);
  } else {
    safe_error(error) = "No Tokenizer named: " + name;
    return false;
  }
}
} // namespace cottontail
