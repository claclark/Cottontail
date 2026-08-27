#ifndef COTTONTAIL_SRC_TOKENIZE_H_
#define COTTONTAIL_SRC_TOKENIZE_H_

#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <vector>

#include "src/core.h"
#include "src/featurizer.h"

namespace cottontail {

class Tokenizer {
public:
  static std::shared_ptr<Tokenizer> make(const std::string &name,
                                         const std::string &recipe,
                                         std::string *error = nullptr);
  static bool check(const std::string &name, const std::string &recipe,
                    std::string *error = nullptr);
  inline std::string recipe() { return recipe_(); }
  inline std::string name() { return name_; }

  std::vector<Token> tokenize(std::shared_ptr<Featurizer> featurizer,
                              const std::string &text) {
    std::unique_ptr<char[]> buffer(new char[text.length() + 1]);
    memcpy(buffer.get(), text.c_str(), text.size());
    buffer.get()[text.size()] = '\0';
    return tokenize(featurizer, buffer.get(), text.size());
  };
  std::vector<Token> tokenize(std::shared_ptr<Featurizer> featurizer,
                              char *buffer, size_t length) {
    // if (destructive()) Expect buffer contents to be completely trashed.
    return tokenize_(featurizer, buffer, length);
  };
  std::string skip(std::string &text, addr n) {
    const char *remainder = skip(text.c_str(), text.length(), n);
    size_t length = text.length() - (remainder - text.c_str());
    return std::string(remainder, length);
  };
  const char *skip(const char *buffer, size_t length, addr n) {
    return skip_(buffer, length, n);
  };
  // Lightweight tokenization: one canonical feature string per token
  // position, preserving order and address alignment.
  std::vector<std::string> split(const std::string &text) {
    return split_(text);
  };
  // Return the number of token positions without requiring callers to
  // materialize the strings returned by split.
  addr count(const std::string &text) { return count_(text); };
  // Return the feature strings best suited to bag-of-words indexing and
  // ranking. A tokenizer may omit null or universal positional placeholders.
  std::vector<std::string> bow(const std::string &text) { return bow_(text); };
  // Return the feature strings needed to express the text as a literal phrase,
  // including any positional placeholders required by the tokenizer.
  std::vector<std::string> phrase(const std::string &text) {
    return phrase_(text);
  };
  bool destructive() { return destructive_(); };

  virtual ~Tokenizer(){};
  Tokenizer(const Tokenizer &) = delete;
  Tokenizer &operator=(const Tokenizer &) = delete;
  Tokenizer(Tokenizer &&) = delete;
  Tokenizer &operator=(Tokenizer &&) = delete;

protected:
  Tokenizer(){};

private:
  virtual std::string recipe_() = 0;
  virtual std::vector<Token> tokenize_(std::shared_ptr<Featurizer> featurizer,
                                       char *buffer, size_t length) = 0;
  virtual const char *skip_(const char *buffer, size_t length, addr n) = 0;
  virtual std::vector<std::string> split_(const std::string &text) = 0;
  // Existing tokenizers inherit projections equivalent to split.
  virtual addr count_(const std::string &text) { return split_(text).size(); };
  virtual std::vector<std::string> bow_(const std::string &text) {
    return split_(text);
  };
  virtual std::vector<std::string> phrase_(const std::string &text) {
    return split_(text);
  };
  virtual bool destructive_() = 0;
  std::string name_ = "";
};
} // namespace cottontail

#endif // COTTONTAIL_SRC_TOKENIZE_H_
