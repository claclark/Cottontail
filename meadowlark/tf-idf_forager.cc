#include "meadowlark/tf-idf_forager.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "src/annotator.h"
#include "src/core.h"
#include "src/tokenizer.h"
#include "src/warren.h"

namespace cottontail {
namespace meadowlark {

std::shared_ptr<Forager>
TfIdfForager::make(const std::map<std::string, std::string> &parameters,
                   std::string *error) {
  std::shared_ptr<TfIdfForager> forager =
      std::shared_ptr<TfIdfForager>(new TfIdfForager());
  return forager;
}

bool TfIdfForager::forage_(std::shared_ptr<Forager> annotator, const std::string &text,
                  const std::vector<Token> &tokens, std::string *error) {
  return true;
}

} // namespace meadowlark
} // namespace cottontail
