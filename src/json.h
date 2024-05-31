#ifndef COTTONTAIL_SRC_JSON_H_
#define COTTONTAIL_SRC_JSON_H_

#include "src/core.h"
#include "src/scribe.h"

#include "external/nlohmann/file/json.hpp"
using json = nlohmann::json;

namespace cottontail {
bool scribe_json(json &j, std::shared_ptr<Scribe> scribe,
                 std::string *error = nullptr);
}
#endif // COTTONTAIL_SRC_JSON_H_
