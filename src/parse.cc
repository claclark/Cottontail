#include "src/parse.h"

#include <iostream>
#include <map>
#include <memory>
#include <string>

#include "src/core.h"
#include "src/featurizer.h"
#include "src/gcl.h"
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
    {"not_containing", NOT_CONTAINING}};

static std::map<enum Operator, std::string> gcl_operator_reverse = {
    {TERM, ""},
    {FIXED, "#"},
    {ONE_OF, "+"},
    {ALL_OF, "^"},
    {FOLLOWED_BY, "..."},
    {CONTAINED_IN, "<<"},
    {CONTAINING, ">>"},
    {NOT_CONTAINED_IN, "!<"},
    {NOT_CONTAINING, "!>"}};

static std::map<enum Operator, unsigned> gcl_operator_min_operands = {
    {TERM, 0},          {FIXED, 0},
    {ONE_OF, 1},        {ALL_OF, 1},
    {FOLLOWED_BY, 2},   {CONTAINED_IN, 2},
    {CONTAINING, 2},    {NOT_CONTAINED_IN, 2},
    {NOT_CONTAINING, 2}};

static std::map<enum Operator, unsigned> gcl_operator_max_operands = {
    {TERM, 0},
    {FIXED, 0},
    {ONE_OF, maxfinity},
    {ALL_OF, maxfinity},
    {FOLLOWED_BY, maxfinity},
    {CONTAINED_IN, maxfinity},
    {CONTAINING, maxfinity},
    {NOT_CONTAINED_IN, maxfinity},
    {NOT_CONTAINING, maxfinity}};

inline bool is_whitespace(char c) { return c == ' ' || c == '\t'; }

inline bool is_term_character(char c) {
  return !is_whitespace(c) && c != ')' && c != '(' && c != '\0';
}

inline bool is_quote_character(char c) {
  return c == '"' || c == '\'' || c == '`';
}

inline bool is_width_character(char c) { return c >= '0' && c <= '9'; }

inline int width_character_value(char c) { return c - '0'; }
} // namespace

const char *parse_expr(const char *where, std::shared_ptr<SExpression> expr,
                       bool *okay) {
  *okay = false;
  while (is_whitespace(*where))
    where++;
  if (*where != '(') {
    if (!is_term_character(*where))
      return where;
    const char *start = where;
    if (is_quote_character(*start)) {
      // quoted term
      char the_quote = *start;
      where++;
      bool escaped = false;
      while (*where != '\0' && (*where != the_quote || escaped)) {
        escaped = (*where == '\\');
        where++;
      }
      if (*where != *start)
        return where;
      where++;
    } else {
      // raw term
      while (is_term_character(*where))
        where++;
    }
    expr->kind_ = Operator::TERM;
    expr->term_ = std::string(start, where);
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
  safe_set(error) = "parse error at offset " + std::to_string(where - s.c_str()) + ":" + s;
  return nullptr;
}

std::string SExpression::to_string() {
  if (kind_ == Operator::TERM)
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
  if (kind_ == TERM && term_.length() > 2 && term_[0] == marker &&
      term_[term_.length() - 1] == marker) {
    std::string phrase = term_.substr(1, term_.length() - 2);
    std::vector<std::string> terms = tokenizer->split(phrase);
    if (terms.size() == 1) {
      std::shared_ptr<SExpression> expr = std::make_shared<SExpression>();
      expr->kind_ = kind_;
      expr->term_ = terms[0];
      expr->width_ = 0;
      return expr;
    } else if (terms.size() > 1) {
      std::string s;
      s += "(>> (# ";
      s += std::to_string(terms.size());
      s += ") (...";
      for (auto &term : terms)
        s += (" " + term);
      s += "))";
      std::string error;
      std::shared_ptr<SExpression> expr = SExpression::from_string(s, &error);
      if (expr != nullptr)
        return expr;
    }
  }
  std::shared_ptr<SExpression> expr = std::make_shared<SExpression>();
  expr->kind_ = kind_;
  expr->term_ = term_;
  expr->width_ = width_;
  for (size_t i = 0; i < subx_.size(); i++)
    expr->subx_.push_back(subx_[i]->expand_phrases(tokenizer, marker));
  return expr;
}

std::shared_ptr<SExpression> SExpression::to_binary() {
  std::shared_ptr<SExpression> expr = std::make_shared<SExpression>();
  expr->kind_ = kind_;
  expr->term_ = term_;
  expr->width_ = width_;
  size_t i = 0;
  for (; i < subx_.size() && i < 2; i++)
    expr->subx_.push_back(subx_[i]->to_binary());
  for (; i < subx_.size(); i++) {
    std::shared_ptr<SExpression> outer = std::make_shared<SExpression>();
    outer->kind_ = kind_;
    outer->term_ = term_;
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
  if (kind_ == FIXED) {
    return std::make_unique<FixedWidthHopper>(width_);
  }
  if (subx_.size() > 2) {
    std::shared_ptr<SExpression> binary_expr = to_binary();
    return binary_expr->to_hopper(featurizer, idx);
  }
  if (subx_.size() < 2)
    return nullptr;
  std::unique_ptr<cottontail::Hopper> left =
      subx_[0]->to_hopper(featurizer, idx);
  std::unique_ptr<cottontail::Hopper> right =
      subx_[1]->to_hopper(featurizer, idx);
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
