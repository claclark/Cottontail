#include "meadowlark/forager.h"

#include <map>
#include <memory>
#include <set>
#include <string>

#include "meadowlark/metadata.h"
#include "meadowlark/null_forager.h"
#include "meadowlark/tf-idf_forager.h"
#include "src/core.h"
#include "src/warren.h"

namespace cottontail {
namespace meadowlark {

namespace {
std::string gcl_string(const std::string &s) {
  std::string quoted = "\"";
  for (char c : s) {
    if (c == '\\' || c == '"')
      quoted += '\\';
    quoted += c;
  }
  quoted += '"';
  return quoted;
}

std::string normalized_name(const std::string &name) {
  return name == "" ? "tf-idf" : name;
}

std::string normalized_tag(const std::string &tag) {
  return tag == "" ? "none" : tag;
}

std::string current_definition_query(const std::string &name,
                                     const std::string &tag) {
  std::string typed = "(>> @ (>> :type: \"forager\"))";
  std::string named =
      "(>> " + typed + " (>> :name: " + gcl_string(name) + "))";
  std::string tagged =
      "(>> " + named + " (>> :tag: " + gcl_string(tag) + "))";
  return "(>> " + tagged + " :query:)";
}

bool check_parameters(const std::map<std::string, std::string> &parameters,
                      std::string *error) {
  static const std::set<std::string> reserved = {
      "file", "filename", "name", "parameters",
      "query", "tag",      "type"};
  for (const auto &parameter : parameters)
    if (reserved.find(parameter.first) != reserved.end()) {
      safe_error(error) = "Reserved forager parameter: " + parameter.first;
      return false;
    }
  return true;
}
} // namespace

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
  if (!check_parameters(parameters, error))
    return nullptr;
  std::shared_ptr<Forager> forager = nullptr;
  std::string n = normalized_name(name);
  std::string t = normalized_tag(tag);
  std::string combined_tag = forager_label(n, t);
  if (n == "null")
    forager = NullForager::make(warren, combined_tag, parameters, error);
  else if (n == "tf-idf")
    forager = TfIdfForager::make(warren, combined_tag, parameters, error);
  else
    safe_error(error) = "No Forager named: " + n;
  if (forager != nullptr)
    forager->warren_ = warren;
  return forager;
}

std::shared_ptr<Forager>
Forager::make(std::shared_ptr<Warren> warren, const std::string &name,
              const std::string &recipe, std::string *error) {
  assert(warren != nullptr);
  std::string n = normalized_name(name);
  std::string t = normalized_tag(recipe);
  std::unique_ptr<Hopper> hopper =
      warren->hopper_from_gcl(current_definition_query(n, t), error);
  if (hopper == nullptr)
    return nullptr;
  addr p, q;
  hopper->tau(minfinity + 1, &p, &q);
  if (p == maxfinity) {
    safe_error(error) = "No current forager definition for: " + n + ":" + t;
    return nullptr;
  }
  ForagerMetadata metadata;
  if (!json2forager(warren->txt()->translate(p, q), &metadata, error))
    return nullptr;
  if (metadata.name != n || normalized_tag(metadata.tag) != t ||
      !metadata.has_query || metadata.has_filename) {
    safe_error(error) = "Forager metadata inconsistency";
    return nullptr;
  }
  return make(warren, n, t, metadata.parameters, error);
}

bool Forager::check(std::shared_ptr<Warren> warren, const std::string &name,
                    const std::string &tag,
                    const std::map<std::string, std::string> &parameters,
                    std::string *error) {
  assert(warren != nullptr);
  return make(warren, name, tag, parameters, error) != nullptr;
}

} // namespace meadowlark
} // namespace cottontail
