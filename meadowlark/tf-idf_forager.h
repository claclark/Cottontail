#ifndef COTTONTAIL_MEADOWLARK_TF_IDF_FORAGER_H_
#define COTTONTAIL_MEADOWLARK_TF_IDF_FORAGER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "meadowlark/forager.h"
#include "src/core.h"
#include "src/stemmer.h"
#include "src/tokenizer.h"
#include "src/warren.h"

namespace cottontail {
namespace meadowlark {

class TfIdfForager : public Forager {
public:
  static std::shared_ptr<Forager>
  make(std::shared_ptr<Warren> warren, const std::string &tag,
       const std::map<std::string, std::string> &parameters,
       std::string *error = nullptr);
  static bool check(const std::string &tag,
                    const std::map<std::string, std::string> &parameters,
                    std::string *error = nullptr);
  virtual ~TfIdfForager(){};
  TfIdfForager(const TfIdfForager &) = delete;
  TfIdfForager &operator=(const TfIdfForager &) = delete;
  TfIdfForager(TfIdfForager &&) = delete;
  TfIdfForager &operator=(TfIdfForager &&) = delete;

private:
  TfIdfForager(){};
  bool forage_(addr p, addr q, std::string *error) final;
  bool ready_() final;
  std::string tag_;
  addr p_min_ = maxfinity;
  addr total_items_ = 0;
  addr total_length_ = 0;
  std::map<addr, addr> df_;
  std::shared_ptr<Stemmer> stemmer_;
  std::map<std::string, std::string> stems_;
  std::shared_ptr<Tokenizer> tokenizer_;
  std::shared_ptr<Featurizer> df_featurizer_;
  std::shared_ptr<Featurizer> tf_featurizer_;
  std::shared_ptr<Featurizer> total_featurizer_;
};

} // namespace meadowlark
} // namespace cottontail

#endif // COTTONTAIL_MEADOWLARK_TF_IDF_FORAGER_H_
