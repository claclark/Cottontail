#ifndef COTTONTAIL_SRC_FASTID_TXT_H_
#define COTTONTAIL_SRC_FASTID_TXT_H_

#include "src/core.h"
#include "src/warren.h"

namespace cottontail {

const std::string FASTID_NAME = "fastid";

// Generate fast id lookup file
bool fastid(std::shared_ptr<Warren> warren, std::string *error = nullptr);

class FastidTxt final : public Txt {
public:
  static std::shared_ptr<Txt> make(const std::string &recipe,
                                   std::shared_ptr<Tokenizer> tokenizer,
                                   std::shared_ptr<Working> working,
                                   std::string *error = nullptr);
  static std::shared_ptr<Txt> make(std::shared_ptr<Txt> txt,
                                   std::shared_ptr<Working> working,
                                   std::string *error = nullptr);
  static bool check(const std::string &recipe, std::string *error = nullptr);

private:
  FastidTxt(){};
  std::string recipe_() final;
  std::string translate_(addr p, addr q) final;
  addr tokens_() final;

  std::shared_ptr<Txt> txt_;
  size_t n_, m_;
  std::unique_ptr<char[]> text_;
  std::unique_ptr<addr[]> p_, q_, offset_;
};

} // namespace cottontail
#endif // COTTONTAIL_SRC_FASTID_TXT_H_
