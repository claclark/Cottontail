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

namespace {
std::unique_ptr<Hopper> container_hopper(std::shared_ptr<Warren> warren,
                                         std::string *error = nullptr) {
  std::string content_key = "container";
  std::string content_query = "";
  if (!warren->get_parameter(content_key, &content_query, error))
    content_query = warren->default_container();
  if (content_query == "") {
    safe_set(error) = "No items to rank defined by warren";
    return nullptr;
  }
  std::unique_ptr<cottontail::Hopper> hopper =
      warren->hopper_from_gcl(content_query, error);
  if (hopper == nullptr) {
    safe_set(error) = "No items to rank defined by warren";
    return nullptr;
  }
  return hopper;
}
} // namespace

std::shared_ptr<Stats> DfStats::make(const std::string &recipe,
                                     std::shared_ptr<Warren> warren,
                                     std::string *error) {
  if (!check(recipe, error))
    return nullptr;
  std::unique_ptr<Hopper> hopper = container_hopper(warren, error);
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
    safe_set(error) = "No global statistics in warren";
    return nullptr;
  }
  addr p, q, n, items = 0;
  for (hopper->tau(minfinity + 1, &p, &q, &n); p < maxfinity;
       hopper->tau(p + 1, &p, &q, &n))
    items += n;
  if (items < 1) {
    safe_set(error) = "No global statistics in warren";
    return nullptr;
  }
  stats->items_ = items;
  hopper = warren->idx()->hopper(featurizer->featurize("length"));
  if (hopper == nullptr) {
    safe_set(error) = "No global statistics in warren";
    return nullptr;
  }
  addr length = 0;
  for (hopper->tau(minfinity + 1, &p, &q, &n); p < maxfinity;
       hopper->tau(p + 1, &p, &q, &n))
    length += n;
  if (length < 1) {
    safe_set(error) = "No global statistics in warren";
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
    safe_set(error) = "Bad recipe for DfStats: " + recipe;
    return false;
  } else {
    return true;
  }
}

std::string DfStats::recipe_() { return ""; }

bool DfStats::have_(const std::string &name) {
  return name == "avgl" || name == "rsj" || name == "idf" || name == "tf";
}

fval DfStats::avgl_() { return average_length_; }

namespace {
inline addr load_df(std::shared_ptr<Warren> warren,
                    std::shared_ptr<Featurizer> featurizer,
                    const std::string &term) {
  return warren->idx()->count(featurizer->featurize(term));
#if 0
  addr p, q, df;
  std::unique_ptr<Hopper> hopper =
      warren->idx()->hopper(featurizer->featurize(term));
  if (hopper == nullptr)
    return 0;
  hopper->tau(minfinity + 1, &p, &q, &df);
  if (p == maxfinity || df < 1)
    return 0;
  else
    return df;
#endif
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
           std::unique_ptr<Hopper> container_hopper)
      : tf_hopper_(std::move(tf_hopper)),
        container_hopper_(std::move(container_hopper)){};

  virtual ~TfHopper(){};
  TfHopper(const TfHopper &) = delete;
  TfHopper &operator=(const TfHopper &) = delete;
  TfHopper(TfHopper &&) = delete;
  TfHopper &operator=(TfHopper &&) = delete;

private:
  void tau_(addr k, addr *p, addr *q, fval *v) final {
    addr p0, q0, df;
    tf_hopper_->tau(k, &p0, &q0, &df);
    container_hopper_->tau(p0, p, q);
    *v = (fval)df;
  };
  void rho_(addr k, addr *p, addr *q, fval *v) final {
    addr p0, q0, df;
    container_hopper_->rho(k, &p0, &q0);
    tf_hopper_->tau(p0, &p0, &q0, &df);
    container_hopper_->tau(p0, p, q);
    *v = (fval)df;
  };
  void uat_(addr k, addr *p, addr *q, fval *v) final {
    addr p0, q0, df;
    container_hopper_->uat(k, &p0, &q0);
    tf_hopper_->ohr(p0, &p0, &q0, &df);
    container_hopper_->ohr(p0, p, q);
    *v = (fval)df;
  };
  void ohr_(addr k, addr *p, addr *q, fval *v) final {
    addr p0, q0, df;
    tf_hopper_->ohr(k, &p0, &q0, &df);
    container_hopper_->ohr(p0, p, q);
    *v = (fval)df;
  };
  std::unique_ptr<Hopper> tf_hopper_;
  std::unique_ptr<Hopper> container_hopper_;
};
} // namespace

std::unique_ptr<Hopper> DfStats::tf_hopper_(const std::string &term) {
  std::unique_ptr<Hopper> tf_hopper =
      warren_->idx()->hopper(tf_featurizer_->featurize(term));
  assert(tf_hopper != nullptr);
  std::unique_ptr<Hopper> chopper = container_hopper(warren_);
  assert(chopper != nullptr);
  return std::make_unique<TfHopper>(std::move(tf_hopper), std::move(chopper));
};
} // namespace cottontail
