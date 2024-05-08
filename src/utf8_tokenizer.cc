#include "src/utf8_tokenizer.h"

#include <iostream>
#include <map>
#include <memory>
#include <string>

#include "src/core.h"
#include "src/unicode.h"

namespace cottontail {

namespace {

constexpr uint32_t UTF8_MAX = 2097152;
constexpr int8_t ACTION_NONTOKEN = 0;
constexpr int8_t ACTION_TOKEN = 1;
constexpr int8_t ACTION_FOLD = 2;
constexpr int8_t ACTION_UNIGRAM = 3;
constexpr int8_t ACTION_BIGRAM = 4;

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

inline bool action_token(const std::string &category) {
  return category == "Lu" || category == "Ll" || category == "Lt" ||
         category == "Mn" || category == "Mc" || category == "Me" ||
         category == "Nd" || category == "Nl" || category == "No" ||
         category == "Lm" || category == "Lo";
}

inline bool action_unigram(const std::string &category) {
  return category == "So";
}

void set_fold_actions(const std::map<uint32_t, uint32_t> &foldings,
                      std::vector<int8_t> *actions) {
  for (auto &folding : foldings)
    if ((*actions)[folding.first] == ACTION_TOKEN)
      (*actions)[folding.first] = ACTION_FOLD;
}

void write_compressed_actions(const std::vector<int8_t> &actions,
                              const std::string &recipe) {
  size_t i = 0;
  std::cout << "struct {\n";
  std::cout << "  size_t n;\n";
  std::cout << "  uint8_t action;\n";
  if (recipe == "")
    std::cout << "} compressed_actions[] = {\n";
  else
    std::cout << "} compressed_actions_" << recipe << "[] = {\n";
  for (;;) {
    size_t j;
    for (j = i + 1; j < actions.size(); j++)
      if (actions[j] != actions[i])
        break;
    std::cout << "  {" << j - i << ", " << (int32_t)actions[i] << "}";
    i = j;
    if (i == actions.size()) {
      std::cout << "};\n";
      break;
    } else {
      std::cout << ",\n";
    }
  }
}

void write_compressed_folds(const std::map<uint32_t, uint32_t> &foldings,
                            const std::string &recipe) {
  std::cout << "struct {\n";
  std::cout << "  uint32_t src, dst;\n";
  if (recipe == "")
    std::cout << "} compressed_folds[] = {\n";
  else
    std::cout << "} compressed_folds_" << recipe << "[] = {\n";
  bool initial = true;
  for (auto &f : foldings) {
    if (initial)
      initial = false;
    else
      std::cout << ",\n";
    std::cout << "  {" << f.first << ", " << f.second << "}";
  }
  std::cout << "};\n";
}

} // namespace

bool utf8_tables(const std::string &unicode_filename,
                 const std::string &folding_filename, const std::string &recipe,
                 std::string *error) {
  std::ifstream unicodef(unicode_filename);
  if (unicodef.fail()) {
    safe_set(error) = "can't open file: " + unicode_filename;
    return false;
  }
  std::ifstream foldingf(folding_filename);
  if (foldingf.fail()) {
    safe_set(error) = "can't open file: " + folding_filename;
    return false;
  }
  std::vector<int8_t> actions;
  for (size_t i = 0; i < UTF8_MAX; i++)
    actions.push_back(ACTION_NONTOKEN);
  bool ideograph = false;
  uint32_t ideograph_start = UTF8_MAX;
  std::string line;
  while (std::getline(unicodef, line)) {
    std::vector<std::string> fields = split(line, ";");
    if (fields.size() == 0)
      continue;
    if (fields.size() < 3) {
      safe_set(error) = "format error:" + unicode_filename + ": " + line;
      return false;
    }
    uint32_t code = decode(fields[0]);
    std::string name = fields[1];
    std::string category = fields[2];
    if (action_token(category)) {
      if (ideograph) {
        for (size_t i = ideograph_start; i <= code; i++)
          actions[i] = ACTION_BIGRAM;
        ideograph = false;
      } else if (name.find("Ideograph") == std::string::npos) {
        actions[code] = ACTION_TOKEN;
      } else {
        ideograph = true;
        ideograph_start = code;
      }
    } else if (action_unigram(category)) {
      actions[code] = ACTION_UNIGRAM;
    }
  }
  std::map<uint32_t, uint32_t> foldings;
  while (std::getline(foldingf, line)) {
    std::vector<std::string> fields = split(line, "; ");
    if (fields.size() == 0)
      continue;
    if (fields.size() < 3) {
      safe_set(error) = "format error:" + folding_filename + ": " + line;
      return false;
    }
    uint32_t source = decode(fields[0]);
    std::string status = fields[1];
    uint32_t target = decode(fields[2]);
    if (status == "C" || status == "S")
      foldings[source] = target;
  };
  set_fold_actions(foldings, &actions);
  write_compressed_actions(actions, recipe);
  std::cout << "\n";
  write_compressed_folds(foldings, recipe);
  return true;
}
} // namespace cottontail
