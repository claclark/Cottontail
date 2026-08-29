#include "src/ngram_tokenizer.h"

#include <memory>
#include <string>
#include <vector>

#include "src/core.h"
#include "src/featurizer.h"
#include "src/ngram_featurizer.h"

namespace cottontail {
namespace {

const std::string number_words[] = {"one", "two",   "three", "four",
                                    "five", "six",   "seven"};
constexpr size_t structural_token_length = 3;

bool parse_recipe(const std::string &recipe, size_t *n) {
  if (recipe == "") {
    *n = 5;
    return true;
  }
  for (size_t i = 0; i < 7; i++)
    if (recipe == number_words[i] ||
        recipe == std::string(1, static_cast<char>('1' + i))) {
      *n = i + 1;
      return true;
    }
  return false;
}

inline bool structural(const char *p, const char *end) {
  // The complete U+FDD0--U+FDEF block is reserved for structural tokens.
  return static_cast<size_t>(end - p) >= structural_token_length &&
         static_cast<unsigned char>(p[0]) == 0xEF &&
         static_cast<unsigned char>(p[1]) == 0xB7 &&
         static_cast<unsigned char>(p[2]) >= 0x90 &&
         static_cast<unsigned char>(p[2]) <= 0xAF;
}

inline const char *next_token(const char *p, const char *end) {
  if (structural(p, end))
    return p + structural_token_length;
  return p + 1;
}

size_t gram_length(const char *p, const char *end, size_t n) {
  size_t length = 0;
  while (length < n && p + length != end && !structural(p + length, end))
    length++;
  return length;
}

std::string gram(const char *p, size_t n) {
  return ngram_marker + std::string(p, n);
}

} // namespace

std::shared_ptr<Tokenizer> NGramTokenizer::make(const std::string &recipe,
                                                std::string *error) {
  size_t n;
  if (parse_recipe(recipe, &n))
    return std::shared_ptr<NGramTokenizer>(new NGramTokenizer(n));
  safe_error(error) = "Can't make NGramTokenizer from recipe: " + recipe;
  return nullptr;
}

bool NGramTokenizer::check(const std::string &recipe, std::string *error) {
  size_t n;
  if (parse_recipe(recipe, &n))
    return true;
  safe_error(error) = "Bad NGramTokenizer recipe: " + recipe;
  return false;
}

std::string NGramTokenizer::recipe_() { return number_words[n_ - 1]; }

std::vector<Token>
NGramTokenizer::tokenize_(std::shared_ptr<Featurizer> featurizer, char *buffer,
                          size_t length) {
  std::vector<Token> tokens;
  char *p = buffer;
  char *end = buffer + length;
  addr address = 0;
  while (p != end) {
    size_t offset = p - buffer;
    if (structural(p, end)) {
      tokens.emplace_back(null_feature, address++, offset,
                          structural_token_length);
      p += structural_token_length;
    } else {
      std::string feature = gram(p, gram_length(p, end, n_));
      tokens.emplace_back(featurizer->featurize(feature), address++, offset, 1);
      p++;
    }
  }
  return tokens;
}

const char *NGramTokenizer::skip_(const char *buffer, size_t length, addr n) {
  const char *p = buffer;
  const char *end = buffer + length;
  while (p != end && n-- > 0)
    p = next_token(p, end);
  return p;
}

std::vector<std::string> NGramTokenizer::split_(const std::string &text) {
  std::vector<std::string> terms;
  const char *p = text.data();
  const char *end = p + text.size();
  while (p != end)
    if (structural(p, end)) {
      terms.emplace_back();
      p += structural_token_length;
    } else {
      terms.push_back(gram(p, gram_length(p, end, n_)));
      p++;
    }
  return terms;
}

addr NGramTokenizer::count_(const std::string &text) {
  addr count = 0;
  const char *p = text.data();
  const char *end = p + text.size();
  while (p != end) {
    p = next_token(p, end);
    count++;
  }
  return count;
}

std::vector<std::string> NGramTokenizer::bow_(const std::string &text) {
  std::vector<std::string> terms;
  const char *p = text.data();
  const char *end = p + text.size();
  while (p != end)
    if (structural(p, end)) {
      p += structural_token_length;
    } else {
      if (gram_length(p, end, n_) == n_)
        terms.push_back(gram(p, n_));
      p++;
    }
  return terms;
}

std::vector<std::string> NGramTokenizer::phrase_(const std::string &text) {
  std::vector<std::string> terms = split_(text);
  bool evidence = false;
  for (std::string &term : terms)
    if (term.size() == ngram_marker.size() + n_ &&
        term.compare(0, ngram_marker.size(), ngram_marker) == 0) {
      evidence = true;
    } else {
      term = universal_marker;
    }
  if (!evidence)
    terms.clear();
  return terms;
}

} // namespace cottontail
