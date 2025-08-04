#include "src/df_stats.h"

#include <cassert>
#include <cmath>
#include <memory>
#include <string>

#include "src/core.h"
#include "src/hopper.h"
#include "src/tagging_featurizer.h"
#include "src/warren.h"

namespace cottontail {

std::shared_ptr<Stats> DfStats::make(const std::string &recipe,
                                     std::shared_ptr<Warren> warren,
                                     std::string *error) {
  if (!check(recipe, error))
    return nullptr;
  std::unique_ptr<Hopper> hopper = content_hopper(warren, error);
  if (hopper == nullptr)
    return nullptr;
  std::shared_ptr<DfStats> stats =
      std::shared_ptr<DfStats>(new DfStats(warren));
  std::shared_ptr<Featurizer> featurizer =
      TaggingFeaturizer::make(warren->featurizer(), "total", error);
  if (featurizer == nullptr)
    return nullptr;
  hopper = warren->idx()->hopper(featurizer->featurize("items"));
  if (hopper == nullptr) {
    safe_error(error) = "No global statistics in warren";
    return nullptr;
  }
  addr p, q, n, items = 0;
  for (hopper->tau(minfinity + 1, &p, &q, &n); p < maxfinity;
       hopper->tau(p + 1, &p, &q, &n))
    items += n;
  if (items < 1) {
    safe_error(error) = "No global statistics in warren";
    return nullptr;
  }
  stats->items_ = items;
  hopper = warren->idx()->hopper(featurizer->featurize("length"));
  if (hopper == nullptr) {
    safe_error(error) = "No global statistics in warren";
    return nullptr;
  }
  addr length = 0;
  for (hopper->tau(minfinity + 1, &p, &q, &n); p < maxfinity;
       hopper->tau(p + 1, &p, &q, &n))
    length += n;
  if (length < 1) {
    safe_error(error) = "No global statistics in warren";
    return nullptr;
  }
  stats->average_length_ = length / stats->items_;
  stats->tf_featurizer_ =
      TaggingFeaturizer::make(warren->featurizer(), "tf", error);
  if (stats->tf_featurizer_ == nullptr)
    return nullptr;
  return stats;
}

bool DfStats::check(const std::string &recipe, std::string *error) {
  if (recipe != "") {
    safe_error(error) = "Bad recipe for DfStats: " + recipe;
    return false;
  } else {
    return true;
  }
}

std::string DfStats::recipe_() { return ""; }

bool DfStats::have_(const std::string &name) {
  if (name == "content") {
    std::string content_query = "";
    if (!warren_->get_parameter("content", &content_query))
      return false;
    return content_query != "";
  }
  return name == "avgl" || name == "rsj" || name == "idf" || name == "tf";
}

fval DfStats::avgl_() { return average_length_; }

namespace {
inline addr load_df(std::shared_ptr<Warren> warren,
                    std::shared_ptr<Featurizer> featurizer,
                    const std::string &term) {
  return warren->idx()->count(featurizer->featurize(term));
}
} // namespace

fval DfStats::idf_(const std::string &term) {
  fval df = (fval)load_df(warren_, tf_featurizer_, term);
  if (df == 0.0)
    return 0.0;
  fval idf = std::log(items_ / df);
  if (idf < 0.0)
    return 0.0;
  return idf;
}

fval DfStats::rsj_(const std::string &term) {
  fval df = (fval)load_df(warren_, tf_featurizer_, term);
  if (df == 0.0)
    return 0.0;
  fval rsj = std::log((items_ - df + 0.5) / (df + 0.5));
  if (rsj < 0.0)
    return 0.0;
  return rsj;
}

namespace {
class TfHopper final : public Hopper {
public:
  TfHopper(std::unique_ptr<Hopper> tf_hopper,
           std::unique_ptr<Hopper> content_hopper)
      : tf_hopper_(std::move(tf_hopper)),
        content_hopper_(std::move(content_hopper)){};

  virtual ~TfHopper(){};
  TfHopper(const TfHopper &) = delete;
  TfHopper &operator=(const TfHopper &) = delete;
  TfHopper(TfHopper &&) = delete;
  TfHopper &operator=(TfHopper &&) = delete;

private:
  void tau_(addr k, addr *p, addr *q, fval *v) final {
    addr p0, q0, tf;
    tf_hopper_->tau(k, &p0, &q0, &tf);
    content_hopper_->tau(p0, p, q);
    *v = (fval)tf;
  };
  void rho_(addr k, addr *p, addr *q, fval *v) final {
    addr p0, q0, tf;
    content_hopper_->rho(k, &p0, &q0);
    tf_hopper_->tau(p0, &p0, &q0, &tf);
    content_hopper_->tau(p0, p, q);
    *v = (fval)tf;
  };
  void uat_(addr k, addr *p, addr *q, fval *v) final {
    addr p0, q0, tf;
    content_hopper_->uat(k, &p0, &q0);
    tf_hopper_->ohr(p0, &p0, &q0, &tf);
    content_hopper_->ohr(p0, p, q);
    *v = (fval)tf;
  };
  void ohr_(addr k, addr *p, addr *q, fval *v) final {
    addr p0, q0, tf;
    tf_hopper_->ohr(k, &p0, &q0, &tf);
    content_hopper_->ohr(p0, p, q);
    *v = (fval)tf;
  };
  std::unique_ptr<Hopper> tf_hopper_;
  std::unique_ptr<Hopper> content_hopper_;
};
} // namespace

std::unique_ptr<Hopper> DfStats::tf_hopper_(const std::string &term) {
  std::unique_ptr<Hopper> tf_hopper =
      warren_->idx()->hopper(tf_featurizer_->featurize(term));
  assert(tf_hopper != nullptr);
  std::unique_ptr<Hopper> chopper = content_hopper(warren_);
  assert(chopper != nullptr);
  return std::make_unique<TfHopper>(std::move(tf_hopper), std::move(chopper));
};
} // namespace cottontail
