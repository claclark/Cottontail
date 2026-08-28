#include "gcl/parse.h"

#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <string>

#include "src/core.h"
#include "src/featurizer.h"
#include "gcl/gcl.h"
#include "gcl/materialize.h"
#include "src/hopper.h"
#include "src/idx.h"

namespace cottontail {

namespace gcl {

namespace {
static std::map<std::string, enum Operator> gcl_operator_forward = {
    {"#", FIXED},
    {"fixed_width", FIXED},
    {"+", ONE_OF},
    {"one_of", ONE_OF},
    {"^", ALL_OF},
    {"all_of", ALL_OF},
    {"...", FOLLOWED_BY},
    {"<>", FOLLOWED_BY},
    {"followed_by", FOLLOWED_BY},
    {"<<", CONTAINED_IN},
    {"contained_in", CONTAINED_IN},
    {">>", CONTAINING},
    {"containing", CONTAINING},
    {"!<", NOT_CONTAINED_IN},
    {"not_contained_in", NOT_CONTAINED_IN},
    {"!>", NOT_CONTAINING},
    {"not_containing", NOT_CONTAINING},
    {"@", LINK},
    {"link", LINK},
    {"materialize", MATERIALIZE}};

static std::map<enum Operator, std::string> gcl_operator_reverse = {
    {TERM, ""},
    {FIXED, "#"},
    {ONE_OF, "+"},
    {ALL_OF, "^"},
    {FOLLOWED_BY, "..."},
    {CONTAINED_IN, "<<"},
    {CONTAINING, ">>"},
    {NOT_CONTAINED_IN, "!<"},
    {NOT_CONTAINING, "!>"},
    {LINK, "@"},
    {MATERIALIZE, "materialize"}};

static std::map<enum Operator, unsigned> gcl_operator_min_operands = {
    {TERM, 0},           {FIXED, 0},
    {ONE_OF, 1},         {ALL_OF, 1},
    {FOLLOWED_BY, 1},    {CONTAINED_IN, 2},
    {CONTAINING, 2},     {NOT_CONTAINED_IN, 2},
    {NOT_CONTAINING, 2}, {LINK, 1},
    {MATERIALIZE, 1}};

static std::map<enum Operator, unsigned> gcl_operator_max_operands = {
    {TERM, 0},
    {FIXED, 0},
    {ONE_OF, maxfinity},
    {ALL_OF, maxfinity},
    {FOLLOWED_BY, maxfinity},
    {CONTAINED_IN, maxfinity},
    {CONTAINING, maxfinity},
    {NOT_CONTAINED_IN, maxfinity},
    {NOT_CONTAINING, maxfinity},
    {LINK, 1},
    {MATERIALIZE, 1}};

inline bool is_whitespace(char c) { return c == ' ' || c == '\t'; }

inline bool is_term_character(char c) {
  return !is_whitespace(c) && c != ')' && c != '(' && c != '\0';
}

inline bool is_quote_character(char c) {
  return c == '"' || c == '\'' || c == '`';
}

inline bool is_width_character(char c) { return c >= '0' && c <= '9'; }

inline int width_character_value(char c) { return c - '0'; }

inline int hex_character_value(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

bool hex_value(const char *where, size_t digits, uint32_t *value) {
  *value = 0;
  for (size_t i = 0; i < digits; i++) {
    int digit = hex_character_value(where[i]);
    if (digit < 0)
      return false;
    *value = 16 * *value + digit;
  }
  return true;
}

bool append_utf8(uint32_t value, std::string *text) {
  if (value == 0 || value > 0x10ffff ||
      (value >= 0xd800 && value <= 0xdfff))
    return false;
  if (value <= 0x7f) {
    text->push_back(static_cast<char>(value));
  } else if (value <= 0x7ff) {
    text->push_back(static_cast<char>(0xc0 | (value >> 6)));
    text->push_back(static_cast<char>(0x80 | (value & 0x3f)));
  } else if (value <= 0xffff) {
    text->push_back(static_cast<char>(0xe0 | (value >> 12)));
    text->push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3f)));
    text->push_back(static_cast<char>(0x80 | (value & 0x3f)));
  } else {
    text->push_back(static_cast<char>(0xf0 | (value >> 18)));
    text->push_back(static_cast<char>(0x80 | ((value >> 12) & 0x3f)));
    text->push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3f)));
    text->push_back(static_cast<char>(0x80 | (value & 0x3f)));
  }
  return true;
}

const char *term_literal(const char *where, std::string *term, bool *okay) {
  *okay = false;
  where++;
  while (*where != '\0' && *where != '|') {
    if (*where == '\n')
      return where;
    if (*where != '\\') {
      term->push_back(*where++);
      continue;
    }
    const char *escape = where++;
    if (*where == '\0' || *where == '\n')
      return escape;
    switch (*where) {
    case '\\':
      term->push_back('\\');
      where++;
      break;
    case '|':
      term->push_back('|');
      where++;
      break;
    case 'a':
      term->push_back('\a');
      where++;
      break;
    case 'b':
      term->push_back('\b');
      where++;
      break;
    case 'f':
      term->push_back('\f');
      where++;
      break;
    case 'n':
      term->push_back('\n');
      where++;
      break;
    case 'r':
      term->push_back('\r');
      where++;
      break;
    case 't':
      term->push_back('\t');
      where++;
      break;
    case 'v':
      term->push_back('\v');
      where++;
      break;
    case 'x': {
      uint32_t value;
      if (!hex_value(where + 1, 2, &value)) {
        term->push_back(*where++);
      } else {
        if (value == 0)
          return escape;
        term->push_back(static_cast<char>(value));
        where += 3;
      }
      break;
    }
    case 'u': {
      uint32_t value;
      if (!hex_value(where + 1, 4, &value)) {
        term->push_back(*where++);
      } else {
        if (!append_utf8(value, term))
          return escape;
        where += 5;
      }
      break;
    }
    case 'U': {
      uint32_t value;
      if (!hex_value(where + 1, 8, &value)) {
        term->push_back(*where++);
      } else {
        if (!append_utf8(value, term))
          return escape;
        where += 9;
      }
      break;
    }
    default:
      term->push_back(*where++);
      break;
    }
  }
  if (*where != '|')
    return where;
  *okay = true;
  return where + 1;
}

// Use raw syntax exactly when it reparses as the same ordinary term.
std::string term_to_gcl(const std::string &term) {
  bool raw = !term.empty() && term[0] != '|' &&
             !is_quote_character(term[0]);
  for (unsigned char c : term)
    if (!is_term_character(static_cast<char>(c)) || c < 0x20 || c == 0x7f) {
      raw = false;
      break;
    }
  if (raw)
    return term;
  static const char hex[] = "0123456789abcdef";
  std::string gcl = "|";
  for (unsigned char c : term) {
    switch (c) {
    case '\\':
      gcl += "\\\\";
      break;
    case '|':
      gcl += "\\|";
      break;
    case '\a':
      gcl += "\\a";
      break;
    case '\b':
      gcl += "\\b";
      break;
    case '\f':
      gcl += "\\f";
      break;
    case '\n':
      gcl += "\\n";
      break;
    case '\r':
      gcl += "\\r";
      break;
    case '\t':
      gcl += "\\t";
      break;
    case '\v':
      gcl += "\\v";
      break;
    default:
      if (c < 0x20 || c == 0x7f) {
        gcl += "\\x";
        gcl.push_back(hex[c >> 4]);
        gcl.push_back(hex[c & 0x0f]);
      } else {
        gcl.push_back(static_cast<char>(c));
      }
      break;
    }
  }
  return gcl + "|";
}

// Phrase quoting currently protects only its delimiter and backslash. Leave
// other backslashes intact for the tokenizer and any later phrase semantics.
std::string phrase_to_string(const std::string &term) {
  std::string phrase;
  if (term.size() < 2)
    return phrase;
  char marker = term.front();
  for (size_t i = 1; i + 1 < term.size(); i++) {
    if (term[i] == '\\' && i + 2 < term.size() &&
        (term[i + 1] == '\\' || term[i + 1] == marker)) {
      phrase.push_back(term[++i]);
    } else {
      phrase.push_back(term[i]);
    }
  }
  return phrase;
}
} // namespace

std::shared_ptr<SExpression> SExpression::make(
    Operator kind, const std::string &term, addr width,
    const std::vector<std::shared_ptr<SExpression>> &subx) {
  std::shared_ptr<SExpression> expr = std::make_shared<SExpression>();
  expr->kind_ = kind;
  expr->term_ = term;
  expr->width_ = width;
  expr->subx_ = subx;
  return expr;
}

const char *parse_expr(const char *where, std::shared_ptr<SExpression> expr,
                       bool *okay) {
  *okay = false;
  while (is_whitespace(*where))
    where++;
  if (*where != '(') {
    if (!is_term_character(*where))
      return where;
    const char *start = where;
    if (*start == '|') {
      bool literal_okay;
      std::string term;
      where = term_literal(where, &term, &literal_okay);
      if (!literal_okay)
        return where;
      expr->kind_ = Operator::TERM;
      expr->term_ = term;
    } else if (is_quote_character(*start)) {
      // quoted term
      char the_quote = *start;
      where++;
      bool escaped = false;
      while (*where != '\0' && (*where != the_quote || escaped)) {
        escaped = (*where == '\\' && !escaped);
        where++;
      }
      if (*where != *start)
        return where;
      where++;
      expr->kind_ = Operator::QUOTE;
      expr->term_ = std::string(start, where);
    } else {
      // raw term
      while (is_term_character(*where))
        where++;
      expr->kind_ = Operator::TERM;
      expr->term_ = std::string(start, where);
    }
    expr->width_ = 0;
    while (is_whitespace(*where))
      where++;
    *okay = true;
    return where;
  }
  where++;
  while (is_whitespace(*where))
    where++;
  if (!is_term_character(*where))
    return where;
  const char *start = where;
  while (is_term_character(*where))
    where++;
  auto opfind = gcl_operator_forward.find(std::string(start, where));
  if (opfind == gcl_operator_forward.end())
    return start;
  expr->kind_ = opfind->second;
  while (is_whitespace(*where))
    where++;
  if (expr->kind_ == Operator::FIXED) {
    if (!is_width_character(*where))
      return where;
    addr width = width_character_value(*where++);
    while (is_width_character(*where))
      width = 10 * width + width_character_value(*where++);
    if (width == 0)
      return where;
    expr->width_ = width;
    while (is_whitespace(*where))
      where++;
    *okay = (*where == ')');
    return where;
  }
  while (*where != ')' && *where != '\0') {
    if (*where == '(') {
      std::shared_ptr<SExpression> subx = std::make_shared<SExpression>();
      bool sub_okay;
      where = parse_expr(where, subx, &sub_okay);
      if (*where != ')' || !sub_okay)
        return where;
      where++;
      expr->subx_.push_back(subx);
    } else if (is_term_character(*where)) {
      std::shared_ptr<SExpression> subx = std::make_shared<SExpression>();
      bool sub_okay;
      where = parse_expr(where, subx, &sub_okay);
      if (!sub_okay)
        return where;
      expr->subx_.push_back(subx);
    } else {
      return where;
    }
    while (is_whitespace(*where))
      where++;
  }
  *okay = (*where == ')' &&
           expr->subx_.size() >= gcl_operator_min_operands[expr->kind_] &&
           expr->subx_.size() <= gcl_operator_max_operands[expr->kind_]);
  expr->term_ = "";
  return where;
}

std::shared_ptr<SExpression> SExpression::from_string(std::string s,
                                                      std::string *error) {
  std::shared_ptr<SExpression> expr = std::make_shared<SExpression>();
  bool okay;
  const char *where = parse_expr(s.c_str(), expr, &okay);
  if (okay)
    return expr;
  safe_error(error) =
      "parse error at offset " + std::to_string(where - s.c_str()) + ":" + s;
  return nullptr;
}

std::shared_ptr<SExpression>
SExpression::make_error(const std::string &message) {
  std::shared_ptr<SExpression> expr = std::make_shared<SExpression>();
  expr->kind_ = ERROR;
  expr->message_ = message;
  expr->width_ = 0;
  return expr;
}

std::string SExpression::to_string() {
  if (kind_ == Operator::ERROR)
    return "ERROR: " + message_;
  if (kind_ == Operator::TERM)
    return term_to_gcl(term_);
  if (kind_ == Operator::QUOTE)
    return term_;
  if (kind_ == Operator::FIXED)
    return "(# " + std::to_string(width_) + ")";
  std::string s = "(" + gcl_operator_reverse[kind_];
  for (size_t i = 0; i < subx_.size(); i++) {
    s += ' ';
    s += subx_[i]->to_string();
  }
  return s + ")";
}

std::shared_ptr<SExpression>
SExpression::expand_phrases(std::shared_ptr<cottontail::Tokenizer> tokenizer,
                            char marker) {
  if (kind_ == ERROR)
    return make_error(message_);
  if (kind_ == QUOTE && term_.length() >= 2 && term_[0] == marker &&
      term_[term_.length() - 1] == marker) {
    std::string phrase = phrase_to_string(term_);
    std::vector<std::string> terms = tokenizer->phrase(phrase);
    if (terms.size() == 0) {
      return make_error("Cannot expand phrase: " + term_);
    } else if (terms.size() == 1) {
      std::shared_ptr<SExpression> expr = std::make_shared<SExpression>();
      expr->kind_ = TERM;
      expr->term_ = terms[0];
      expr->width_ = 0;
      return expr;
    } else if (terms.size() > 1) {
      std::string s;
      s += "(>> (# ";
      s += std::to_string(terms.size());
      s += ") (...";
      for (auto &term : terms)
        s += (" " + term_to_gcl(term));
      s += "))";
      std::string error;
      std::shared_ptr<SExpression> expr = SExpression::from_string(s, &error);
      if (expr != nullptr)
        return expr;
      return make_error("Cannot expand phrase " + term_ + ": " + error);
    }
  }
  std::shared_ptr<SExpression> expr = std::make_shared<SExpression>();
  expr->kind_ = kind_;
  expr->term_ = term_;
  expr->message_ = message_;
  expr->width_ = width_;
  for (size_t i = 0; i < subx_.size(); i++) {
    std::shared_ptr<SExpression> subx =
        subx_[i]->expand_phrases(tokenizer, marker);
    if (subx->is_error())
      return subx;
    expr->subx_.push_back(subx);
  }
  return expr;
}

std::shared_ptr<SExpression> SExpression::to_binary() {
  if (kind_ == ERROR)
    return make_error(message_);
  std::shared_ptr<SExpression> expr = std::make_shared<SExpression>();
  expr->kind_ = kind_;
  expr->term_ = term_;
  expr->message_ = message_;
  expr->width_ = width_;
  size_t i = 0;
  for (; i < subx_.size() && i < 2; i++)
    expr->subx_.push_back(subx_[i]->to_binary());
  for (; i < subx_.size(); i++) {
    std::shared_ptr<SExpression> outer = std::make_shared<SExpression>();
    outer->kind_ = kind_;
    outer->term_ = term_;
    outer->message_ = message_;
    outer->width_ = width_;
    outer->subx_.push_back(expr);
    outer->subx_.push_back(subx_[i]->to_binary());
    expr = outer;
  }
  return expr;
}

std::unique_ptr<cottontail::Hopper>
SExpression::to_hopper(std::shared_ptr<Featurizer> featurizer,
                       std::shared_ptr<Idx> idx) {
  if (kind_ == TERM) {
    return idx->hopper(featurizer->featurize(term_));
  }
  if (kind_ == QUOTE || kind_ == ERROR)
    return nullptr;
  if (kind_ == FIXED) {
    if (width_ == 1)
      return std::make_unique<UniversalHopper>();
    else
      return std::make_unique<FixedWidthHopper>(width_);
  }
  if (kind_ == LINK) {
    if (subx_.size() != 1)
      return nullptr;
    std::unique_ptr<cottontail::Hopper> expr =
        subx_[0]->to_hopper(featurizer, idx);
    if (expr == nullptr)
      return nullptr;
    return std::make_unique<cottontail::gcl::Link>(std::move(expr));
  }
  if (kind_ == MATERIALIZE) {
    if (subx_.size() != 1)
      return nullptr;
    std::unique_ptr<cottontail::Hopper> expr =
        subx_[0]->to_hopper(featurizer, idx);
    if (expr == nullptr)
      return nullptr;
    return std::make_unique<cottontail::gcl::Materialize>(std::move(expr));
  }
  if (subx_.size() > 2) {
    std::shared_ptr<SExpression> binary_expr = to_binary();
    return binary_expr->to_hopper(featurizer, idx);
  }
  if (subx_.size() == 1 &&
      (kind_ == ONE_OF || kind_ == ALL_OF || kind_ == FOLLOWED_BY))
    return subx_[0]->to_hopper(featurizer, idx);
  if (subx_.size() < 2)
    return nullptr;
  std::unique_ptr<cottontail::Hopper> left =
      subx_[0]->to_hopper(featurizer, idx);
  std::unique_ptr<cottontail::Hopper> right =
      subx_[1]->to_hopper(featurizer, idx);
  if (left == nullptr || right == nullptr)
    return nullptr;
  switch (kind_) {
  case ONE_OF:
    return std::make_unique<cottontail::gcl::Or>(std::move(left),
                                                 std::move(right));
  case ALL_OF:
    return std::make_unique<cottontail::gcl::And>(std::move(left),
                                                  std::move(right));
    break;
  case FOLLOWED_BY:
    return std::make_unique<cottontail::gcl::FollowedBy>(std::move(left),
                                                         std::move(right));
    break;
  case CONTAINED_IN:
    return std::make_unique<cottontail::gcl::ContainedIn>(std::move(left),
                                                          std::move(right));
    break;
  case CONTAINING:
    return std::make_unique<cottontail::gcl::Containing>(std::move(left),
                                                         std::move(right));
    break;
  case NOT_CONTAINED_IN:
    return std::make_unique<cottontail::gcl::NotContainedIn>(std::move(left),
                                                             std::move(right));
    break;
  case NOT_CONTAINING:
    return std::make_unique<cottontail::gcl::NotContaining>(std::move(left),
                                                            std::move(right));
    break;
  default:
    return nullptr;
    break;
  }
}
} // namespace gcl
} // namespace cottontail
