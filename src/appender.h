#ifndef COTTONTAIL_SRC_APPENDER_H_
#define COTTONTAIL_SRC_APPENDER_H_

#include <memory>
#include <string>

#include "src/annotator.h"
#include "src/committable.h"
#include "src/core.h"
#include "src/featurizer.h"
#include "src/tokenizer.h"
#include "src/working.h"

namespace cottontail {

class Appender : public Committable {
public:
  static std::shared_ptr<Appender>
  make(const std::string &name, const std::string &recipe,
       std::string *error = nullptr, std::shared_ptr<Working> working = nullptr,
       std::shared_ptr<Featurizer> featurizer = nullptr,
       std::shared_ptr<Tokenizer> tokenizer = nullptr,
       std::shared_ptr<Annotator> annotator = nullptr);
  static bool check(const std::string &name, const std::string &recipe,
                    std::string *error = nullptr);
  static bool recover(const std::string &name, const std::string &recipe,
                      bool commit, std::string *error = nullptr,
                      std::shared_ptr<Working> working = nullptr);
  inline std::string recipe() { return recipe_(); }
  inline std::string name() { return name_; }

  bool append(const std::string &text, addr *p, addr *q,
              std::string *error = nullptr) {
    return append_(text, p, q, error);
  };

  virtual ~Appender(){};
  Appender(const Appender &) = delete;
  Appender &operator=(const Appender &) = delete;
  Appender(Appender &&) = delete;
  Appender &operator=(Appender &&) = delete;

protected:
  Appender(){};

private:
  virtual std::string recipe_() = 0;
  virtual bool append_(const std::string &text, addr *p, addr *q,
                       std::string *error) = 0;
  std::string name_ = "";
};

} // namespace cottontail

#endif // COTTONTAIL_SRC_APPENDER_H_
