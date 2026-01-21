#ifndef COTTONTAIL_MEADOWLARK_LARK_STATS_H_
#define COTTONTAIL_MEADOWLARK_LARK_STATS_H_

#include <memory>
#include <string>

#include "src/core.h"
#include "src/hopper.h"
#include "src/stats.h"
#include "src/warren.h"

namespace cottontail {

class LarkStats final : public Stats {
public:
  static std::shared_ptr<Stats> make(const std::string &recipe,
                                     std::shared_ptr<Warren> warren,
                                     std::string *error = nullptr);
  static bool check(const std::string &recipe, std::string *error = nullptr);

  virtual ~LarkStats(){};
  LarkStats(const LarkStats &) = delete;
  LarkStats &operator=(const LarkStats &) = delete;
  LarkStats(LarkStats &&) = delete;
  LarkStats &operator=(LarkStats &&) = delete;

protected:
  LarkStats(std::shared_ptr<Warren> warren) : Stats(warren){};

private:
  std::string recipe_() final;
  bool have_(const std::string &name) final;
  fval avgl_() final;
  fval idf_(const std::string &term) final;
  fval rsj_(const std::string &term) final;
  std::unique_ptr<Hopper> tf_hopper_(const std::string &term) final;
  fval items_;
  fval average_length_;
  std::shared_ptr<Featurizer> tf_featurizer_;
};

} // namespace cottontail
#endif // COTTONTAIL_MEADOWLARK_LARK_STATS_H_
