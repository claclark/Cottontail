#ifndef COTTONTAIL_SRC_ASCII_TOKENIZER_H_
#define COTTONTAIL_SRC_ASCII_TOKENIZER_H_

#include <memory>
#include <string>
#include <vector>

#include "src/core.h"
#include "src/tokenizer.h"

namespace cottontail {
// The xml flag in the constructor turns on crude and very limited support
// for XML/SGML/HTML, tailored towards older TREC collections.
// If we see something that looks like the start of an XML tag,
// we keep only the tag name as a token of the form "<NAME>" or "</NAME>",
// so "<ITAG tagnum=10>" becomes just "<ITAG>".
// On the other hand we try to filter out other XML/SGML/HTML crud.

class AsciiTokenizer final : public Tokenizer {
public:
  AsciiTokenizer(bool xml = false) : xml_(xml){};
  static std::shared_ptr<Tokenizer> make(const std::string &recipe,
                                         std::string *error = nullptr);
  static std::shared_ptr<Tokenizer> make(bool xml = false) {
    std::string recipe;
    if (xml)
      recipe = "xml";
    else
      recipe = "noxml";
    return make(recipe);
  };
  static bool check(const std::string &recipe, std::string *error = nullptr);

  virtual ~AsciiTokenizer(){};
  AsciiTokenizer(const AsciiTokenizer &) = delete;
  AsciiTokenizer &operator=(const AsciiTokenizer &) = delete;
  AsciiTokenizer(AsciiTokenizer &&) = delete;
  AsciiTokenizer &operator=(AsciiTokenizer &&) = delete;

private:
  bool xml_;
  std::string recipe_() final;
  std::vector<Token> tokenize_(std::shared_ptr<Featurizer> featurizer,
                               char *buffer, size_t length) final;
  const char *skip_(const char *buffer, size_t length, addr n) final;
  std::vector<std::string> split_(const std::string &text) final;
  bool destructive_() final { return true; };
};

} // namespace cottontail

#endif // COTTONTAIL_SRC_ASCII_TOKENIZER_H_
