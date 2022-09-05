#ifndef COTTONTAIL_SRC_SIMPLE_WARREN_H_
#define COTTONTAIL_SRC_SIMPLE_WARREN_H_

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

class SimpleWarren final : public Warren {
public:
  static std::shared_ptr<Warren> make(const std::string &burrow,
                                      std::string *error = nullptr);

  virtual ~SimpleWarren(){};
  SimpleWarren(const SimpleWarren &) = delete;
  SimpleWarren &operator=(const SimpleWarren &) = delete;
  SimpleWarren(SimpleWarren &&) = delete;
  SimpleWarren &operator=(SimpleWarren &&) = delete;

private:
  SimpleWarren(std::shared_ptr<Working> working,
               std::shared_ptr<Featurizer> featurizer,
               std::shared_ptr<Tokenizer> tokenizer, std::shared_ptr<Idx> idx,
               std::shared_ptr<Txt> txt)
      : Warren(working, featurizer, tokenizer, idx, txt){};
  bool set_parameter_(const std::string &key, const std::string &value,
                      std::string *error) final;
  bool get_parameter_(const std::string &key, std::string *value,
                      std::string *error) final;
};

} // namespace cottontail
#endif // COTTONTAIL_SRC_SIMPLE_WARREN_H_
