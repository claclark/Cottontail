#include "src/utf8_tokenizer.h"

#include <iostream>
#include <map>
#include <memory>
#include <string>

#include "src/core.h"

namespace cottontail {

namespace {
std::vector<std::string> split(const std::string &line,
                               const std::string &separator) {
  size_t start = 0;
  size_t end = line.find('#');
  if (end == std::string::npos)
    end = line.length();
  std::vector<std::string> fields;
  for (;;) {
    size_t found = line.substr(start, end).find(separator);
    if (found == std::string::npos)
      return fields;
    fields.emplace_back(line.substr(start, found));
    start += found + separator.length();
  }
}

inline uint32_t dehex(char c) {
  switch (c) {
  case '0':
    return 0;
  case '1':
    return 1;
  case '2':
    return 2;
  case '3':
    return 3;
  case '4':
    return 4;
  case '5':
    return 5;
  case '6':
    return 6;
  case '7':
    return 7;
  case '8':
    return 8;
  case '9':
    return 9;
  case 'a':
  case 'A':
    return 10;
  case 'b':
  case 'B':
    return 11;
  case 'c':
  case 'C':
    return 12;
  case 'd':
  case 'D':
    return 13;
  case 'e':
  case 'E':
    return 14;
  case 'f':
  case 'F':
    return 15;
  default:
    return -1;
  }
}

inline uint32_t decode(const std::string &code) {
  uint32_t v = 0;
  for (const char *s = code.c_str(); *s; s++) {
    int32_t d = dehex(*s);
    if (d == -1)
      return -1;
    v = v * 16 + d;
  }
  return v;
}

constexpr uint32_t UTF8_MAX = 2097152;
constexpr int8_t ACTION_NONTOKEN = -1;
constexpr int8_t ACTION_TOKEN = 0;
constexpr int8_t ACTION_UNIGRAM = 1;
constexpr int8_t ACTION_BIGRAM = 2;

inline bool action_token(const std::string &category) {
  return category == "Lu" || category == "Ll" || category == "Lt" ||
         category == "Mn" || category == "Mc" || category == "Me" ||
         category == "Nd" || category == "Nl" || category == "No" ||
         category == "Lm" || category == "Lo";
}

inline bool action_unigram(const std::string &category) {
  return category == "So";
}

} // namespace

bool utf8_tables(const std::string &unicode_filename,
                 const std::string &folding_filename, std::string *error) {
  std::vector<int8_t> action;
  std::vector<std::string> comment;
  for (size_t i = 0; i < UTF8_MAX; i++) {
    action.push_back(ACTION_NONTOKEN);
    comment.push_back("");
  }
  std::ifstream unicodef(unicode_filename);
  if (unicodef.fail()) {
    safe_set(error) = "can't open file: " + unicode_filename;
    return false;
  }
  bool ideograph = false;
  uint32_t ideograph_start = UTF8_MAX;
  std::string line;
  while (std::getline(unicodef, line)) {
    std::vector<std::string> fields = split(line, ";");
    if (fields.size() < 3) {
      safe_set(error) = "format error: " + line;
      return false;
    }
    uint32_t code = decode(fields[0]);
    std::string name = fields[1];
    std::string category = fields[2];
    if (action_token(category)) {
      if (ideograph) {
        for (size_t i = ideograph_start; i <= code; i++)
          action[i] = ACTION_BIGRAM;
        ideograph = false;
      } else if (name.find("Ideograph") == std::string::npos) {
        action[code] = ACTION_TOKEN;
      } else {
        ideograph = true;
        ideograph_start = code;
      }
    } else if (action_unigram(category)) {
      action[code] = ACTION_UNIGRAM;
    }
    comment[code] = line;
  }
  std::cout << "action[] = {\n";
  for (size_t i = 0; i < action.size(); i++) {
    std::cout << "  " << (int) action[i];
    if (i + i < action.size())
      std::cout << ",";
    if (comment[i] != "")
      std::cout << "\t// " << comment[i];
    std::cout << "\n";
  }
  std::cout << "};\n";
  return true;
}
} // namespace cottontail
