#ifndef COTTONTAIL_MEADOWLARK_TF_IDF_STATS_H_
#define COTTONTAIL_MEADOWLARK_TF_IDF_STATS_H_

#include <memory>
#include <string>

#include "src/core.h"
#include "src/hopper.h"
#include "src/stats.h"
#include "src/warren.h"

namespace cottontail {
namespace meadowlark {

class TfIdfStats final : public Stats {
public:
  static std::shared_ptr<Stats> make(const std::string &recipe,
                                     std::shared_ptr<Warren> warren,
                                     std::string *error = nullptr);
  static bool check(const std::string &recipe, std::string *error = nullptr);

  virtual ~TfIdfStats(){};
  TfIdfStats(const TfIdfStats &) = delete;
  TfIdfStats &operator=(const TfIdfStats &) = delete;
  TfIdfStats(TfIdfStats &&) = delete;
  TfIdfStats &operator=(TfIdfStats &&) = delete;

protected:
  TfIdfStats(std::shared_ptr<Warren> warren) : Stats(warren){};

private:
  std::string recipe_() final;
  bool have_(const std::string &name) final;
  fval avgl_() final;
  fval idf_(const std::string &term) final;
  fval rsj_(const std::string &term) final;
  std::unique_ptr<Hopper> tf_hopper_(const std::string &term) final;
  std::unique_ptr<Hopper> container_hopper_() final;
  std::string tag_;
  std::string label_;
  std::string id_query_;
  std::string content_query_;
  std::string container_query_;
  std::shared_ptr<Stemmer> stemmer_;
  std::shared_ptr<Tokenizer> tokenizer_;
  fval items_;
  fval average_length_;
  std::shared_ptr<Featurizer> tf_featurizer_;
};

} // namespace meadowlake
} // namespace cottontail
#endif // COTTONTAIL_MEADOWLARK_TF_IDF_STATS_H_
