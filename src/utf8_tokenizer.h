#ifndef COTTONTAIL_SRC_UTF8_TOKENIZER_H_
#define COTTONTAIL_SRC_UTF8_TOKENIZER_H_

#include <memory>
#include <string>
#include <vector>

#include "src/core.h"
#include "src/tokenizer.h"

namespace cottontail {

class Utf8Tokenizer final : public Tokenizer {
public:
  static std::shared_ptr<Tokenizer> make(const std::string &recipe,
                                         std::string *error = nullptr);
  static bool check(const std::string &recipe, std::string *error = nullptr);

  virtual ~Utf8Tokenizer(){};
  Utf8Tokenizer(const Utf8Tokenizer &) = delete;
  Utf8Tokenizer &operator=(const Utf8Tokenizer &) = delete;
  Utf8Tokenizer(Utf8Tokenizer &&) = delete;
  Utf8Tokenizer &operator=(Utf8Tokenizer &&) = delete;

private:
  Utf8Tokenizer (){};
  std::string recipe_() final;
  std::vector<Token> tokenize_(std::shared_ptr<Featurizer> featurizer,
                               char *buffer, size_t length) final;
  const char *skip_(const char *buffer, size_t length, addr n) final;
  std::vector<std::string> split_(const std::string &text) final;
  bool destructive_() final { return true; };
};

bool utf8_tables(const std::string &unicode_filename,
                 const std::string &folding_filename, const std::string &recipe,
                 std::string *error = nullptr);
} // namespace cottontail

#endif // COTTONTAIL_SRC_UTF8_TOKENIZER_H_
