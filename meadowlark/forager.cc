#include "meadowlark/forager.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "meadowlark/null_forager.h"
#include "meadowlark/tf-idf_forager.h"
#include "src/annotator.h"
#include "src/core.h"
#include "src/tokenizer.h"
#include "src/warren.h"

namespace cottontail {
namespace meadowlark {
std::shared_ptr<Forager>
Forager::make(std::shared_ptr<Featurizer> featurizer, const std::string &name,
              const std::string &tag,
              const std::map<std::string, std::string> &parameters,
              std::string *error) {
  std::shared_ptr<Forager> forager = nullptr;
  std::string combined_tag;
  if (name != "")
    combined_tag = name + ":";
  if (tag != "")
    combined_tag += tag + ":";
  if (featurizer == nullptr)
    safe_error(error) = "Featurizer can't be a nullptr";
  else if (name == "null")
    forager = NullForager::make(featurizer, combined_tag, parameters, error);
  else if (name == "" || name == "tf-idf")
    forager = TfIdfForager::make(featurizer, combined_tag, parameters, error);
  else
    safe_error(error) = "No Forager named: " + name;
  return forager;
}

} // namespace meadowlark
} // namespace cottontail
