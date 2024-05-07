#ifndef COTTONTAIL_SRC_UTF8_TOKENIZER_H_
#define COTTONTAIL_SRC_UTF8_TOKENIZER_H_

#include <memory>
#include <string>
#include <vector>

#include "src/core.h"
#include "src/tokenizer.h"

namespace cottontail {

bool utf8_tables(const std::string &unicode_filename,
                 const std::string &folding_filename,
                 std::string *error = nullptr);
} // namespace cottontail

#endif // COTTONTAIL_SRC_UTF8_TOKENIZER_H_
