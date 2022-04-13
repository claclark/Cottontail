#ifndef COTTONTAIL_SRC_TXT_H_
#define COTTONTAIL_SRC_TXT_H_

#include "src/core.h"
#include "src/tokenizer.h"
#include "src/working.h"

namespace cottontail {

class Txt {
public:
  static std::shared_ptr<Txt>
  make(const std::string &name, const std::string &recipe,
       std::string *error = nullptr,
       std::shared_ptr<Tokenizer> tokenizer = nullptr,
       std::shared_ptr<Working> working = nullptr);
  static bool check(const std::string &name, const std::string &recipe,
                    std::string *error = nullptr);
  inline std::string recipe() { return recipe_(); }
  inline std::string name() { return name_; }

  inline std::string translate(addr p, addr q) { return translate_(p, q); }
  inline addr tokens() { return tokens_(); }

  virtual ~Txt(){};
  Txt(const Txt &) = delete;
  Txt &operator=(const Txt &) = delete;
  Txt(Txt &&) = delete;
  Txt &operator=(Txt &&) = delete;

protected:
  Txt(){};

private:
  virtual std::string recipe_() = 0;
  virtual std::string translate_(addr p, addr q) = 0;
  virtual addr tokens_() = 0;
  std::string name_ = "";
};

} // namespace cottontail
#endif // COTTONTAIL_SRC_TXT_H_
