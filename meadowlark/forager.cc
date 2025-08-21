#include "meadowlark/forager.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "src/annotator.h"
#include "src/core.h"
#include "src/tokenizer.h"
#include "src/warren.h"
#include "meadowlark/null_forager.h"
#include "meadowlark/tf-idf_forager.h"

namespace cottontail {
namespace meadowlark {
std::shared_ptr<Forager>
Forager::make(const std::string &name, const std::string &tag,
              const std::map<std::string, std::string> &parameters,
              std::string *error)
{
  std::shared_ptr<Forager> forager = nullptr;
  if (name == "" || name == "null")
     forager =  NullForager::make(parameters, error);
  else if (name == "tf-idf")
     forager  = TfIdfForager::make(parameters, error);
  else
    safe_error(error) = "No Forager named: " + name;
  if (forager != nullptr)
    forager->tag_ = name + ":" + tag + ":";
  return forager;
}

}
} // namespace cottontail
