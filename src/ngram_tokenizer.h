#ifndef COTTONTAIL_SRC_NGRAM_TOKENIZER_H_
#define COTTONTAIL_SRC_NGRAM_TOKENIZER_H_

#include <memory>
#include <string>
#include <vector>

#include "src/core.h"
#include "src/tokenizer.h"

namespace cottontail {

class NGramTokenizer final : public Tokenizer {
public:
  static std::shared_ptr<Tokenizer> make(const std::string &recipe,
                                         std::string *error = nullptr);
  static bool check(const std::string &recipe, std::string *error = nullptr);

  virtual ~NGramTokenizer(){};
  NGramTokenizer(const NGramTokenizer &) = delete;
  NGramTokenizer &operator=(const NGramTokenizer &) = delete;
  NGramTokenizer(NGramTokenizer &&) = delete;
  NGramTokenizer &operator=(NGramTokenizer &&) = delete;

private:
  explicit NGramTokenizer(size_t n) : n_(n){};

  size_t n_;
  std::string recipe_() final;
  std::vector<Token> tokenize_(std::shared_ptr<Featurizer> featurizer,
                               char *buffer, size_t length) final;
  const char *skip_(const char *buffer, size_t length, addr n) final;
  std::vector<std::string> split_(const std::string &text) final;
  addr count_(const std::string &text) final;
  std::vector<std::string> bow_(const std::string &text) final;
  std::vector<std::string> phrase_(const std::string &text) final;
  bool destructive_() final { return false; };
};

} // namespace cottontail

#endif // COTTONTAIL_SRC_NGRAM_TOKENIZER_H_
