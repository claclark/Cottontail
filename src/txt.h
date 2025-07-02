#ifndef COTTONTAIL_SRC_TXT_H_
#define COTTONTAIL_SRC_TXT_H_

#include "src/committable.h"
#include "src/core.h"
#include "src/tokenizer.h"
#include "src/working.h"

namespace cottontail {

class Txt : public Committable {
public:
  static std::shared_ptr<Txt>
  make(const std::string &name, const std::string &recipe,
       std::string *error = nullptr,
       std::shared_ptr<Tokenizer> tokenizer = nullptr,
       std::shared_ptr<Working> working = nullptr);
  static bool check(const std::string &name, const std::string &recipe,
                    std::string *error = nullptr);
  static std::shared_ptr<Txt> wrap(const std::string &recipe,
                                   std::shared_ptr<Txt> txt,
                                   std::string *error = nullptr);

  inline std::string name() { return name_(); }
  inline std::string recipe() { return recipe_(); }
  inline std::shared_ptr<Txt> clone(std::string *error = nullptr) {
    return clone_(error);
  }
  inline std::string translate(addr p, addr q) { return translate_(p, q); }
  inline std::string raw(addr p, addr q) { return raw_(p, q); }
  inline addr tokens() { return tokens_(); }
  inline bool range(addr *p, addr *q) { return range_(p, q); };

  virtual ~Txt(){};
  Txt(const Txt &) = delete;
  Txt &operator=(const Txt &) = delete;
  Txt(Txt &&) = delete;
  Txt &operator=(Txt &&) = delete;

protected:
  Txt(){};

private:
  virtual std::string name_() = 0;
  virtual std::string recipe_() = 0;
  virtual std::shared_ptr<Txt> clone_(std::string *error);
  virtual std::string translate_(addr p, addr q) = 0;
  virtual std::string raw_(addr p, addr q) { return translate(p, q); }
  virtual addr tokens_() = 0;
  virtual bool range_(addr *p, addr *q) {
    addr t = tokens();
    if (t == 0)
      return false;
    *p = 0;
    *q = t - 1;
    return true;
  }
};

} // namespace cottontail
#endif // COTTONTAIL_SRC_TXT_H_
