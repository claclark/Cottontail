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

namespace {
std::string file_metadata(const std::string &type, const std::string &file) {
  json metadata;
  metadata["filename"] = file;
  metadata["type"] = type;
  return metadata.dump(2, ' ', false, json::error_handler_t::replace) + "\n";
}

bool parse_metadata(const std::string &text, json *metadata,
                    std::string *error) {
  assert(metadata != nullptr);
  size_t end = text.size();
  while (end > 0 && text[end - 1] == '\0')
    --end;
  std::string encoded(text.begin(), text.begin() + end);
  std::string decoded = encoded;
  if (encoded.find(open_object_token) != std::string::npos &&
      !json_convert(encoded, &decoded, error))
    return false;
  try {
    *metadata = json::parse(decoded);
  } catch (const json::parse_error &) {
    safe_error(error) = "Error parsing metadata from json";
    return false;
  }
  return true;
}
} // namespace

std::string json_metadata(const std::string &file) {
  return file_metadata("json", file);
}

std::string text_metadata(const std::string &file) {
  return file_metadata("text", file);
}

std::string code_metadata(const std::string &file) {
  return file_metadata("code", file);
}

std::string tsv_metadata(const std::string &file,
                         const std::string &separator, bool header,
                         const std::vector<std::string> &headings,
                         const std::vector<std::string> &features) {
  assert(headings.size() == features.size());
  json metadata;
  metadata["columns"] = json::array();
  metadata["filename"] = file;
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
                         const std::string &query,
                         const std::map<std::string, std::string> &parameters) {
  json metadata;
  metadata["name"] = name;
  metadata["tag"] = tag;
  metadata["query"] = query;
  metadata["parameters"] = parameters;
  metadata["type"] = "forager";
  return metadata.dump(2, ' ', false, json::error_handler_t::replace) + "\n";
}

std::string forager_file2json(const std::string &filename,
                              const std::string &name,
                              const std::string &tag) {
  json metadata;
  metadata["filename"] = filename;
  metadata["name"] = name;
  metadata["tag"] = tag;
  metadata["type"] = "forager";
  return metadata.dump(2, ' ', false, json::error_handler_t::replace) + "\n";
}

bool json2forager(const std::string &text, ForagerMetadata *record,
                  std::string *error) {
  if (record == nullptr) {
    safe_error(error) = "Error parsing forager from json";
    return false;
  }

  *record = ForagerMetadata();

  json metadata;
  if (!parse_metadata(text, &metadata, error)) {
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
  if ((metadata.contains("query") && !metadata["query"].is_string()) ||
      (metadata.contains("filename") &&
       !metadata["filename"].is_string())) {
    safe_error(error) = "Error parsing forager from json";
    return false;
  }

  record->name = metadata["name"].get<std::string>();
  if (metadata.contains("tag"))
    record->tag = metadata["tag"].get<std::string>();
  if (metadata.contains("query")) {
    record->has_query = true;
    record->query = metadata["query"].get<std::string>();
  }
  if (metadata.contains("filename")) {
    record->has_filename = true;
    record->filename = metadata["filename"].get<std::string>();
  }

  if (metadata.contains("parameters") && metadata["parameters"].is_object()) {
    for (const auto &parameter : metadata["parameters"].items()) {
      if (!parameter.value().is_string()) {
        safe_error(error) = "Error parsing forager from json";
        return false;
      }
      record->parameters[parameter.key()] =
          parameter.value().get<std::string>();
    }
  }
  return true;
}

bool json2forager(const std::string &text, std::string *name, std::string *tag,
                  std::map<std::string, std::string> *parameters,
                  std::string *error) {
  if (name == nullptr || tag == nullptr || parameters == nullptr) {
    safe_error(error) = "Error parsing forager from json";
    return false;
  }
  ForagerMetadata metadata;
  if (!json2forager(text, &metadata, error))
    return false;
  *name = metadata.name;
  *tag = metadata.tag;
  *parameters = metadata.parameters;
  return true;
}

bool json2file(const std::string &text, std::string *type,
               std::string *filename, std::string *error) {
  if (type == nullptr || filename == nullptr) {
    safe_error(error) = "Error parsing file metadata from json";
    return false;
  }
  *type = "";
  *filename = "";
  json metadata;
  if (!parse_metadata(text, &metadata, error) || !metadata.is_object() ||
      !metadata.contains("type") || !metadata["type"].is_string()) {
    safe_error(error) = "Error parsing file metadata from json";
    return false;
  }
  *type = metadata["type"].get<std::string>();
  if (metadata.contains("filename")) {
    if (!metadata["filename"].is_string()) {
      safe_error(error) = "Error parsing file metadata from json";
      return false;
    }
    *filename = metadata["filename"].get<std::string>();
  } else if (metadata.contains("file")) {
    if (!metadata["file"].is_string()) {
      safe_error(error) = "Error parsing file metadata from json";
      return false;
    }
    *filename = metadata["file"].get<std::string>();
  }
  return true;
}

} // namespace meadowlark
} // namespace cottontail
