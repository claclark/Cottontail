#ifndef COTTONTAIL_SRC_JSON_H_
#define COTTONTAIL_SRC_JSON_H_

#include "src/core.h"
#include "src/scribe.h"

#include "external/nlohmann/file/json.hpp"
using json = nlohmann::json;

namespace cottontail {

const std::string open_object_token = "\xEF\xB7\x90";
const std::string close_object_token = "\xEF\xB7\x91";
const std::string open_array_token = "\xEF\xB7\x92";
const std::string close_array_token = "\xEF\xB7\x93";
const std::string open_string_token = "\xEF\xB7\x94";
const std::string close_string_token = "\xEF\xB7\x95";

bool scribe_json(json &j, std::shared_ptr<Scribe> scribe,
                 std::string *error = nullptr);
}
#endif // COTTONTAIL_SRC_JSON_H_
