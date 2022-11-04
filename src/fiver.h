#ifndef COTTONTAIL_SRC_FIVER_H_
#define COTTONTAIL_SRC_FIVER_H_

#include <map>
#include <string>

#include "src/annotator.h"
#include "src/appender.h"
#include "src/core.h"
#include "src/featurizer.h"
#include "src/idx.h"
#include "src/tokenizer.h"
#include "src/txt.h"
#include "src/warren.h"
#include "src/working.h"

namespace cottontail {

class Fiver final : public Warren {
public:
  static std::shared_ptr<Fiver> make(
      std::shared_ptr<Working> working, std::shared_ptr<Featurizer> featurizer,
      std::shared_ptr<Tokenizer> tokenizer, addr identifier,
      std::string *error = nullptr,
      std::shared_ptr<std::map<std::string, std::string>> parameters = nullptr);

  virtual ~Fiver(){};
  Fiver(const Fiver &) = delete;
  Fiver &operator=(const Fiver &) = delete;
  Fiver(Fiver &&) = delete;
  Fiver &operator=(Fiver &&) = delete;

private:
  Fiver(std::shared_ptr<Working> working,
        std::shared_ptr<Featurizer> featurizer,
        std::shared_ptr<Tokenizer> tokenizer, std::shared_ptr<Idx> idx,
        std::shared_ptr<Txt> txt)
      : Warren(working, featurizer, tokenizer, idx, txt){};
  bool set_parameter_(const std::string &key, const std::string &value,
                      std::string *error) final {
    safe_set(error) = "Fiver can't set its parameters";
    return false;
  };
  bool get_parameter_(const std::string &key, std::string *value,
                      std::string *error) final {
    if (parameters_ == nullptr) {
      *value = "";
      return true;
    }
    std::map<std::string, std::string>::iterator item = parameters_->find(key);
    if (item != parameters_->end())
      *value = item->second;
    else
      *value = "";
    return true;
  };

  std::shared_ptr<std::vector<std::string>> appends_;
  std::shared_ptr<std::vector<Annotation>> annotations_;
  std::shared_ptr<std::map<std::string, std::string>> parameters_;
};

} // namespace cottontail
#endif // COTTONTAIL_SRC_FIVER_H_
