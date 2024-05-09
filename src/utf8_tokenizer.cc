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

inline uint32_t dehex(const std::string &codepoint) {
  uint32_t v = 0;
  for (const char *s = codepoint.c_str(); *s; s++) {
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
  std::cout << "struct CompressedAction {\n";
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
  std::cout << "struct CompressedFold {\n";
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

inline void utf8(uint32_t codepoint, std::vector<uint8_t> *s) {
  if (codepoint <= 0x7F) {
    s->push_back((uint8_t)codepoint);
  } else if (codepoint <= 0x7FF) {
    s->push_back((uint8_t)((codepoint >> 6) | 0xC0));
    s->push_back((uint8_t)((codepoint & 0x3F) | 0x80));
  } else if (codepoint <= 0xFFFF) {
    s->push_back((uint8_t)((codepoint >> 12) | 0xE0));
    s->push_back((uint8_t)(((codepoint >> 6) & 0x3F) | 0x80));
    s->push_back((uint8_t)((codepoint & 0x3F) | 0x80));
  } else if (codepoint <= 0x1FFFFF) {
    s->push_back((uint8_t)((codepoint >> 18) | 0xF0));
    s->push_back((uint8_t)(((codepoint >> 12) & 0x3F) | 0x80));
    s->push_back((uint8_t)(((codepoint >> 6) & 0x3F) | 0x80));
    s->push_back((uint8_t)((codepoint & 0x3F) | 0x80));
  } else {
    s->push_back((uint8_t)0x20);
  }
}

inline const uint8_t *unicode(const uint8_t *s, uint32_t *codepoint) {
  if ((s[0] & 0x80) == 0x00) {
    *codepoint = s[0];
    return s + 1;
  } else if (((s[0] & 0xE0) == 0xC0) && ((s[1] & 0xC0) == 0x80)) {
    *codepoint = (((uint32_t)(s[0] & 0x1F)) << 6) | ((uint32_t)(s[1] & 0x3F));
    return s + 2;
  } else if (((s[0] & 0xF0) == 0XE0) && ((s[1] & 0xC0) == 0x80) &&
             ((s[2] & 0xC0) == 0x80)) {
    *codepoint = (((uint32_t)(s[0] & 0x0F)) << 12) |
                 (((uint32_t)(s[1] & 0x3F)) << 6) | ((uint32_t)(s[2] & 0x3F));
    return s + 3;
  } else if (((*s & 0xF8) == 0XF0) && ((s[1] & 0xC0) == 0x80) &&
             ((s[2] & 0xC0) == 0x80) && ((s[3] & 0xC0) == 0x80)) {
    *codepoint = (((uint32_t)(s[0] & 0x07)) << 18) |
                 (((uint32_t)(s[1] & 0x3F)) << 12) |
                 (((uint32_t)(s[2] & 0x3F)) << 6) | ((uint32_t)(s[3] & 0x3F));
    return s + 4;
  } else {
    *codepoint = 0x20;
    return s + 1;
  }
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
    uint32_t codepoint = dehex(fields[0]);
    std::string name = fields[1];
    std::string category = fields[2];
    if (action_token(category)) {
      if (ideograph) {
        for (size_t i = ideograph_start; i <= codepoint; i++)
          actions[i] = ACTION_BIGRAM;
        ideograph = false;
      } else if (name.find("Ideograph") == std::string::npos) {
        actions[codepoint] = ACTION_TOKEN;
      } else {
        ideograph = true;
        ideograph_start = codepoint;
      }
    } else if (action_unigram(category)) {
      actions[codepoint] = ACTION_UNIGRAM;
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
    uint32_t source = dehex(fields[0]);
    std::string status = fields[1];
    uint32_t target = dehex(fields[2]);
    if (status == "C" || status == "S")
      foldings[source] = target;
  };
  set_fold_actions(foldings, &actions);
  write_compressed_actions(actions, recipe);
  std::cout << "\n";
  write_compressed_folds(foldings, recipe);
  return true;
}

std::mutex Utf8Tokenizer::lock_;
uint8_t Utf8Tokenizer::action_token_ = '\0';
uint8_t Utf8Tokenizer::action_unigram_ = '\0';
uint8_t Utf8Tokenizer::action_bigram_ = '\0';
uint8_t **Utf8Tokenizer::actions_ = nullptr;
uint8_t *Utf8Tokenizer::folds_ = nullptr;

Utf8Tokenizer::Utf8Tokenizer() {
  lock_.lock();
  if (actions_ == nullptr) {
    std::vector<uint8_t> folds;
    std::map<uint32_t, size_t> offset;
    size_t n = sizeof(compressed_folds) / sizeof(CompressedFold);
    for (size_t i = 0; i < n; i++) {
      offset[compressed_folds[i].src] = folds.size();
      utf8(compressed_folds[i].dst, &folds);
    }
    folds_ = (uint8_t *)malloc(sizeof(uint8_t) * (folds.size() + 1));
    folds_[folds.size()] = '\0';
    for (size_t i = 0; i < folds.size(); i++)
      folds_[i] = folds[i];
    actions_ = (uint8_t **)malloc(sizeof(uint8_t *) * UTF8_MAX);
    size_t m = sizeof(compressed_actions) / sizeof(CompressedAction);
    uint32_t codepoint = 0;
    for (size_t i = 0; i < m; i++)
      for (size_t j = 0; j < compressed_actions[i].n; j++) {
        switch (compressed_actions[i].action) {
        case ACTION_NONTOKEN:
          actions_[codepoint] = nullptr;
          break;
        case ACTION_TOKEN:
          actions_[codepoint] = &action_token_;
          break;
        case ACTION_FOLD:
          actions_[codepoint] = folds_ + offset[codepoint];
          break;
        case ACTION_UNIGRAM:
          actions_[codepoint] = &action_unigram_;
          break;
        case ACTION_BIGRAM:
          actions_[codepoint] = &action_bigram_;
          break;
        default:
          assert(false);
          break;
        }
        codepoint++;
      }
    assert(codepoint == UTF8_MAX);
  }
  lock_.unlock();
}

std::shared_ptr<Tokenizer> Utf8Tokenizer::make(const std::string &recipe,
                                               std::string *error) {
  if (recipe == "") {
    std::shared_ptr<Utf8Tokenizer> tokenizer =
        std::shared_ptr<Utf8Tokenizer>(new Utf8Tokenizer());
    return tokenizer;
  } else {
    safe_set(error) = "Can't make Utf8Tokenizer from recipe: " + recipe;
    return nullptr;
  }
}

bool Utf8Tokenizer::check(const std::string &recipe, std::string *error) {
  if (recipe == "") {
    return true;
  } else {
    safe_set(error) = "Bad Utf8Tokenizer recipe: " + recipe;
    return false;
  }
}

std::string Utf8Tokenizer::recipe_() { return ""; }

std::vector<Token>
Utf8Tokenizer::tokenize_(std::shared_ptr<Featurizer> featurizer, char *buffer,
                         size_t length) {
  std::vector<Token> tokens;
  return tokens;
}

const char *Utf8Tokenizer::skip_(const char *buffer, size_t length, addr n) {
  return buffer;
}

std::vector<std::string> Utf8Tokenizer::split_(const std::string &text) {
  std::vector<std::string> tokens;
  int state = ACTION_NONTOKEN;
  const uint8_t *s = (const uint8_t *)text.c_str();
  std::string token;
  while (*s) {
    uint32_t codepoint;
    const uint8_t *t = unicode(s, &codepoint);
    uint8_t *action = actions_[codepoint];
    switch (state) {
    case ACTION_NONTOKEN:
      if (action != nullptr) {
        if (action == &action_token_) {
          state = ACTION_TOKEN;
          for (const uint8_t *u = s; u < t; u++)
            token.push_back(*u);
        } else if (action == &action_bigram_) {
          state = ACTION_BIGRAM;
          for (const uint8_t *u = s; u < t; u++)
            token.push_back(*u);
        } else if (action == &action_unigram_) {
          for (const uint8_t *u = s; u < t; u++)
            token.push_back(*u);
          tokens.push_back(token);
          token = "";
        } else {
          state = ACTION_TOKEN;
          for (token.push_back(*action++); (*action & 0xC0) == 0x80;
               token.push_back(*action++))
            ;
          ;
        }
      }
      break;
    case ACTION_TOKEN:
      if (action == nullptr) {
        state = ACTION_NONTOKEN;
        tokens.push_back(token);
        token = "";
      } else if (action == &action_token_) {
        for (const uint8_t *u = s; u < t; u++)
          token.push_back(*u);
      } else if (action == &action_bigram_) {
        state = ACTION_BIGRAM;
        tokens.push_back(token);
        token = "";
        for (const uint8_t *u = s; u < t; u++)
          token.push_back(*u);
      } else if (action == &action_unigram_) {
        state = ACTION_NONTOKEN;
        tokens.push_back(token);
        token = "";
        for (const uint8_t *u = s; u < t; u++)
          token.push_back(*u);
        tokens.push_back(token);
        token = "";
      } else {
        for (token.push_back(*action++); (*action & 0xC0) == 0x80;
             token.push_back(*action++))
          ;
      }
      break;
    case ACTION_BIGRAM:
      if (action == nullptr) {
        state = ACTION_NONTOKEN;
        token = "";
      } else if (action == &action_token_) {
        state = ACTION_TOKEN;
        token = "";
        for (const uint8_t *u = s; u < t; u++)
          token.push_back(*u);
      } else if (action == &action_bigram_) {
        for (const uint8_t *u = s; u < t; u++)
          token.push_back(*u);
        tokens.push_back(token);
        token = "";
        for (const uint8_t *u = s; u < t; u++)
          token.push_back(*u);
      } else if (action == &action_unigram_) {
        state = ACTION_NONTOKEN;
        token = "";
        for (const uint8_t *u = s; u < t; u++)
          token.push_back(*u);
        tokens.push_back(token);
        token = "";
      } else {
        state = ACTION_TOKEN;
        token = "";
        for (token.push_back(*action++); (*action & 0xC0) == 0x80;
             token.push_back(*action++))
          ;
      }
      break;
    default:
      assert(false);
      break;
    }
    s = t;
  }
  if (state == ACTION_TOKEN)
    tokens.push_back(token);
  return tokens;
}

} // namespace cottontail