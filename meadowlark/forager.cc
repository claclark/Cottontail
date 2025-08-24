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
Forager::make(std::shared_ptr<Warren> warren, const std::string &name,
              const std::string &tag,
              const std::map<std::string, std::string> &parameters,
              std::string *error) {
  std::shared_ptr<Forager> forager = nullptr;
  std::string combined_tag;
  if (name != "")
    combined_tag = name + ":";
  if (tag != "")
    combined_tag += tag + ":";
  if (name == "null")
    forager = NullForager::make(warren, combined_tag, parameters, error);
  else if (name == "" || name == "tf-idf" || name == "tfidf" ||
           name == "tf_idf")
    forager = TfIdfForager::make(warren, combined_tag, parameters, error);
  else
    safe_error(error) = "No Forager named: " + name;
  if (forager != nullptr)
    forager->warren_ = warren;
  return forager;
}

} // namespace meadowlark
} // namespace cottontail
