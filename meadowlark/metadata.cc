#include "meadowlark/metadata.h"

#include <cassert>
#include <map>
#include <string>
#include <vector>

#include "src/core.h"
#include "src/json.h"
#include "src/nlohmann.h"

namespace cottontail {
namespace meadowlark {

std::string json_metadata(const std::string &file) {
  json metadata;
  metadata["file"] = file;
  metadata["type"] = "json";
  return metadata.dump(2, ' ', false, json::error_handler_t::replace) + "\n";
}

std::string tsv_metadata(const std::string &file,
                         const std::string &separator, bool header,
                         const std::vector<std::string> &headings,
                         const std::vector<std::string> &features) {
  assert(headings.size() == features.size());
  json metadata;
  metadata["columns"] = json::array();
  metadata["file"] = file;
  metadata["header"] = header;
  metadata["separator"] = separator;
  metadata["type"] = "tsv";
  for (size_t i = 0; i < features.size(); i++) {
    json column;
    column["feature"] = features[i];
    if (header)
      column["header"] = headings[i];
    column["index"] = i;
    metadata["columns"].push_back(column);
  }
  return metadata.dump(2, ' ', false, json::error_handler_t::replace) + "\n";
}

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

  std::string decoded = text;
  if (text.find(open_object_token) != std::string::npos)
    decoded = json_translate(text);
  size_t end = decoded.size();
  while (end > 0 && decoded[end - 1] == '\0')
    --end;

  json metadata;
  try {
    metadata = json::parse(decoded.begin(), decoded.begin() + end);
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
