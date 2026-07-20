#include "meadowlark/metadata.h"

#include <map>
#include <string>

#include "src/core.h"
#include "src/nlohmann.h"

namespace cottontail {
namespace meadowlark {

std::string forager2json(const std::string &name, const std::string &tag,
                         const std::map<std::string, std::string> &parameters) {
  json metadata;
  metadata["name"] = name;
  metadata["tag"] = tag;
  metadata["parameters"] = parameters;
  metadata["type"] = "forager";
  return metadata.dump(2, ' ', false, json::error_handler_t::replace) + "\n";
}

bool json2forager(const std::string &text, std::string *name, std::string *tag,
                  std::map<std::string, std::string> *parameters,
                  std::string *error) {
  if (name == nullptr || tag == nullptr || parameters == nullptr) {
    safe_error(error) = "Error parsing forager from json";
    return false;
  }

  *name = "";
  *tag = "";
  parameters->clear();

  size_t end = text.size();
  while (end > 0 && text[end - 1] == '\0')
    --end;

  json metadata;
  try {
    metadata = json::parse(text.begin(), text.begin() + end);
  } catch (const json::parse_error &) {
    safe_error(error) = "Error parsing forager from json";
    return false;
  }

  if (!metadata.is_object() || !metadata.contains("name") ||
      !metadata["name"].is_string() ||
      (metadata.contains("tag") && !metadata["tag"].is_string())) {
    safe_error(error) = "Error parsing forager from json";
    return false;
  }
  if (metadata.contains("type") &&
      (!metadata["type"].is_string() || metadata["type"] != "forager")) {
    safe_error(error) = "Metadata type is not forager";
    return false;
  }

  *name = metadata["name"].get<std::string>();
  if (metadata.contains("tag"))
    *tag = metadata["tag"].get<std::string>();

  if (metadata.contains("parameters") && metadata["parameters"].is_object()) {
    for (const auto &parameter : metadata["parameters"].items()) {
      if (!parameter.value().is_string()) {
        safe_error(error) = "Error parsing forager from json";
        return false;
      }
      (*parameters)[parameter.key()] = parameter.value().get<std::string>();
    }
  }
  return true;
}

} // namespace meadowlark
} // namespace cottontail
