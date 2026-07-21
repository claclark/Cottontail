#include "meadowlark/forager.h"

#include <map>
#include <memory>
#include <string>

#include "meadowlark/metadata.h"
#include "meadowlark/null_forager.h"
#include "meadowlark/tf-idf_forager.h"
#include "src/annotator.h"
#include "src/core.h"
#include "src/json.h"
#include "src/tokenizer.h"
#include "src/warren.h"

namespace cottontail {
namespace meadowlark {

std::string forager_label(const std::string &name, const std::string &tag) {
  std::string combined_tag;
  if (name != "")
    combined_tag = name + ":";
  if (tag != "")
    combined_tag += tag + ":";
  if (combined_tag == "")
    combined_tag = "tf-idf:";
  return combined_tag;
}

std::shared_ptr<Forager>
Forager::make(std::shared_ptr<Warren> warren, const std::string &name,
              const std::string &tag,
              const std::map<std::string, std::string> &parameters,
              std::string *error) {
  std::shared_ptr<Forager> forager = nullptr;
  std::string normalized_name = name == "" ? "tf-idf" : name;
  std::string normalized_tag = tag == "" ? "none" : tag;
  std::string combined_tag = forager_label(normalized_name, normalized_tag);
  if (normalized_name == "null")
    forager = NullForager::make(warren, combined_tag, parameters, error);
  else if (normalized_name == "tf-idf")
    forager = TfIdfForager::make(warren, combined_tag, parameters, error);
  else
    safe_error(error) = "No Forager named: " + normalized_name;
  if (forager != nullptr) {
    forager->name_ = normalized_name;
    forager->tag_ = normalized_tag;
    forager->parameters_ = parameters;
    forager->warren_ = warren;
  }
  return forager;
}

bool Forager::check(const std::string &name, const std::string &tag,
                    const std::map<std::string, std::string> &parameters,
                    std::string *error) {
  std::string normalized_name = name == "" ? "tf-idf" : name;
  std::string normalized_tag = tag == "" ? "none" : tag;
  std::string combined_tag = forager_label(normalized_name, normalized_tag);
  if (normalized_name == "null")
    return NullForager::check(combined_tag, parameters, error);
  else if (normalized_name == "tf-idf")
    return TfIdfForager::check(combined_tag, parameters, error);
  safe_error(error) = "No Forager named: " + normalized_name;
  return false;
}

bool Forager::label(std::string *error) {
  addr p, q;
  return json_append(forager2json(name_, tag_, parameters_), warren_, &p, &q,
                     "@", error);
}

} // namespace meadowlark
} // namespace cottontail
