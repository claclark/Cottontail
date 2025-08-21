#ifndef COTTONTAIL_MEADOWLARK_TF_IDF_FORAGER_H_
#define COTTONTAIL_MEADOWLARK_TF_IDF_FORAGER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "meadowlark/forager.h"
#include "src/annotator.h"
#include "src/core.h"
#include "src/tokenizer.h"
#include "src/warren.h"

namespace cottontail {
namespace meadowlark {

class TfIdfForager : public Forager {
public:
  static std::shared_ptr<Forager>
  make(const std::map<std::string, std::string> &parameters,
       std::string *error = nullptr);
  static std::shared_ptr<Forager> make(const std::string &tag,
                                       std::string *error = nullptr) {
    std::map<std::string, std::string> parameters;
    return make(parameters, error);
  };
  virtual ~TfIdfForager(){};
  TfIdfForager(const TfIdfForager &) = delete;
  TfIdfForager &operator=(const TfIdfForager &) = delete;
  TfIdfForager(TfIdfForager &&) = delete;
  TfIdfForager &operator=(TfIdfForager &&) = delete;

private:
  TfIdfForager() {};
  bool forage_(std::shared_ptr<Forager> annotator, const std::string &text,
               const std::vector<Token> &tokens, std::string *error) final;
};
} // namespace meadowlark
} // namespace cottontail

#endif // COTTONTAIL_MEADOWLARK_TF_IDF_FORAGER_H_
