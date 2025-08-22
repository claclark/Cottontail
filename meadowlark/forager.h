#ifndef COTTONTAIL_MEADOWLARK_FORAGER_H_
#define COTTONTAIL_MEADOWLARK_FORAGER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <mutex>

#include "src/annotator.h"
#include "src/core.h"
#include "src/featurizer.h"
#include "src/tokenizer.h"
#include "src/warren.h"

namespace cottontail {
namespace meadowlark {

class Forager {
public:
  static std::shared_ptr<Forager>
  make(std::shared_ptr<Featurizer> featurizer, const std::string &name,
       const std::string &tag,
       const std::map<std::string, std::string> &parameters,
       std::string *error = nullptr);
  static std::shared_ptr<Forager> make(std::shared_ptr<Featurizer> featurizer,
                                       const std::string &name,
                                       const std::string &tag,
                                       std::string *error = nullptr) {
    std::map<std::string, std::string> parameters;
    return make(featurizer, name, tag, parameters, error);
  };
  inline bool forage(std::shared_ptr<Forager> annotator,
                     const std::string &text, const std::vector<Token> &tokens,
                     std::string *error = nullptr) {
    std::lock_guard<std::mutex> _(mutex_);
    return forage_(annotator, text, tokens, error);
  };

  virtual ~Forager(){};
  Forager(const Forager &) = delete;
  Forager &operator=(const Forager &) = delete;
  Forager(Forager &&) = delete;
  Forager &operator=(Forager &&) = delete;

protected:
  Forager(){};
  std::mutex mutex_;

private:
  virtual bool forage_(std::shared_ptr<Forager> annotator,
                       const std::string &text,
                       const std::vector<Token> &tokens,
                       std::string *error) = 0;
};
} // namespace meadowlark
} // namespace cottontail

#endif // COTTONTAIL_MEADOWLARK_FORAGER_H_
