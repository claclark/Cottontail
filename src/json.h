#ifndef COTTONTAIL_SRC_JSON_H_
#define COTTONTAIL_SRC_JSON_H_

#include "src/core.h"
#include "src/scribe.h"

#include "external/nlohmann/file/json.hpp"
using json = nlohmann::json;

namespace cottontail {

// unicode noncharacters are tokens in the utf8_tokenizer
// https://www.unicode.org/faq/private_use.html
constexpr size_t noncharacter_token_length = 3; // for the ones below
const std::string open_object_token = "\xEF\xB7\x90";
const std::string close_object_token = "\xEF\xB7\x91";
const std::string open_array_token = "\xEF\xB7\x92";
const std::string close_array_token = "\xEF\xB7\x93";
const std::string open_string_token = "\xEF\xB7\x94";
const std::string close_string_token = "\xEF\xB7\x95";
const std::string colon_token = "\xEF\xB7\x96";
const std::string comma_token = "\xEF\xB7\x97";

bool json_scribe(json &j, std::shared_ptr<Scribe> scribe,
                 std::string *error = nullptr);
std::string json_translate(const std::string &s);
} // namespace cottontail
#endif // COTTONTAIL_SRC_JSON_H_
