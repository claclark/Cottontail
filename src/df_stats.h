#ifndef COTTONTAIL_SRC_DF_STATS_H_
#define COTTONTAIL_SRC_DF_STATS_H_

#include <memory>
#include <string>

#include "src/core.h"
#include "src/hopper.h"
#include "src/stats.h"
#include "src/warren.h"

namespace cottontail {

class DfStats final : public Stats {
public:
  static std::shared_ptr<Stats> make(const std::string &recipe,
                                     std::shared_ptr<Warren> warren,
                                     std::string *error = nullptr);
  static bool check(const std::string &recipe, std::string *error = nullptr);

  virtual ~DfStats(){};
  DfStats(const DfStats &) = delete;
  DfStats &operator=(const DfStats &) = delete;
  DfStats(DfStats &&) = delete;
  DfStats &operator=(DfStats &&) = delete;

protected:
  DfStats(std::shared_ptr<Warren> warren) : Stats(warren){};

private:
  std::string recipe_() final;
  virtual bool have_(const std::string &name) final;
  virtual fval avgl_() final;
  virtual fval idf_(const std::string &term) final;
  virtual fval rsj_(const std::string &term) final;
  virtual std::unique_ptr<Hopper> tf_hopper_(const std::string &term) final;
  fval items_;
  fval average_length_;
  std::shared_ptr<Featurizer> tf_featurizer_;
};

} // namespace cottontail
#endif // COTTONTAIL_SRC_DF_STATS_H_
