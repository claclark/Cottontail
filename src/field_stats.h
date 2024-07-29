#ifndef COTTONTAIL_SRC_FIELD_STATS_H_
#define COTTONTAIL_SRC_FIELD_STATS_H_

#include <memory>
#include <string>
#include <vector>

#include "src/core.h"
#include "src/hopper.h"
#include "src/stats.h"
#include "src/warren.h"

namespace cottontail {

class FieldStats final : public Stats {
public:
  static std::shared_ptr<Stats> make(const std::string &recipe,
                                     std::shared_ptr<Warren> warren,
                                     std::string *error = nullptr);
  static bool check(const std::string &recipe, std::string *error = nullptr);

  virtual ~FieldStats(){};
  FieldStats(const FieldStats &) = delete;
  FieldStats &operator=(const FieldStats &) = delete;
  FieldStats(FieldStats &&) = delete;
  FieldStats &operator=(FieldStats &&) = delete;

protected:
  FieldStats(std::shared_ptr<Warren> warren) : Stats(warren){};

private:
  std::string recipe_() final;
  bool have_(const std::string &name) final;
  fval avgl_() final;
  fval idf_(const std::string &term) final;
  fval rsj_(const std::string &term) final;
  std::unique_ptr<Hopper> tf_hopper_(const std::string &term) final;
  fval items_;
  fval average_length_;
  std::vector<fval> weights_;
  std::vector<std::shared_ptr<Featurizer>> tf_featurizers_;
  std::shared_ptr<Featurizer> df_featurizer_;
};

} // namespace cottontail
#endif // COTTONTAIL_SRC_FIELD_STATS_H_
