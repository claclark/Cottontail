#include "src/field_stats.h"

#include <cassert>
#include <cmath>
#include <memory>
#include <regex>
#include <string>
#include <vector>

#include "src/core.h"
#include "src/hopper.h"
#include "src/tagging_featurizer.h"
#include "src/warren.h"

namespace cottontail {

std::shared_ptr<Stats> FieldStats::make(const std::string &recipe,
                                        std::shared_ptr<Warren> warren,
                                        std::string *error) {
  if (!check(recipe, error))
    return nullptr;
  std::unique_ptr<Hopper> hopper = content_hopper(warren, error);
  if (hopper == nullptr)
    return nullptr;
  std::shared_ptr<FieldStats> stats =
      std::shared_ptr<FieldStats>(new FieldStats(warren));
  std::string fields_parameter;
  if (!warren->get_parameter("fields", &fields_parameter, error))
    return nullptr;
  std::regex tab("\t");
  std::vector<std::string> field_queries{
      std::sregex_token_iterator(fields_parameter.begin(),
                                 fields_parameter.end(), tab, -1),
      {}};
  if (field_queries.size() == 0) {
    safe_set(error) = "No fields defined by warren";
    return nullptr;
  }
  std::string weights_parameter;
  if (!warren->get_parameter("weights", &weights_parameter, error))
    return nullptr;
  std::vector<std::string> weight_strings{
      std::sregex_token_iterator(weights_parameter.begin(),
                                 weights_parameter.end(), tab, -1),
      {}};
  if (weight_strings.size() == 0)
    return Stats::make("df", recipe, warren, error);
  if (weight_strings.size() != field_queries.size()) {
    safe_set(error) = "Warren fields and weights have inconsistent lengths";
    return nullptr;
  }
  for (size_t i = 0; i < weight_strings.size(); i++)
    try {
      fval w = std::stof(weight_strings[i]);
      if (w < 0.0) {
        safe_set(error) = "Negative field weight";
        return nullptr;
      }
      if (w > 0.0) {
        std::shared_ptr<Featurizer> featurizer = TaggingFeaturizer::make(
            warren->featurizer(), "tf:" + std::to_string(i), error);
        if (featurizer == nullptr)
          return nullptr;
        stats->weights_.push_back(w);
        stats->tf_featurizers_.push_back(featurizer);
      }
    } catch (const std::invalid_argument &e) {
      safe_set(error) = "Non-numberic field weight";
      return nullptr;
    }
  if (stats->weights_.size() == 0) {
    safe_set(error) = "No positive field weights";
    return nullptr;
  }
  std::shared_ptr<Featurizer> df_featurizer =
      TaggingFeaturizer::make(warren->featurizer(), "df", error);
  if (df_featurizer == nullptr)
    return nullptr;
  stats->df_featurizer_ = df_featurizer;
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
  return stats;
}

bool FieldStats::check(const std::string &recipe, std::string *error) {
  if (recipe != "") {
    safe_set(error) = "Bad recipe for FieldStats: " + recipe;
    return false;
  } else {
    return true;
  }
}

std::string FieldStats::recipe_() { return ""; }

bool FieldStats::have_(const std::string &name) {
  if (name == "content") {
    std::string content_query = "";
    if (!warren_->get_parameter("content", &content_query))
      return false;
    return content_query != "";
  }
  return name == "avgl" || name == "rsj" || name == "idf" || name == "tf";
}

fval FieldStats::avgl_() { return average_length_; }

namespace {
inline fval load_df(std::shared_ptr<Warren> warren,
                    std::shared_ptr<Featurizer> featurizer,
                    const std::string &term) {
  std::unique_ptr<Hopper> hopper =
      warren->idx()->hopper(featurizer->featurize(term));
  addr p, q, v;
  hopper->tau(0, &p, &q, &v);
  return (fval)v;
}
} // namespace

fval FieldStats::idf_(const std::string &term) {
  fval df = load_df(warren_, df_featurizer_, term);
  if (df == 0.0)
    return 0.0;
  fval idf = std::log(items_ / df);
  if (idf < 0.0)
    return 0.0;
  return idf;
}

fval FieldStats::rsj_(const std::string &term) {
  fval df = load_df(warren_, df_featurizer_, term);
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
  TfHopper(std::vector<std::unique_ptr<Hopper>> *tf_hoppers,
           const std::vector<fval> &weights,
           std::unique_ptr<Hopper> *content_hopper) {
    assert(tf_hoppers != nullptr);
    assert(tf_hoppers->size() > 0);
    assert(tf_hoppers->size() == weights.size());
    assert(content_hopper != nullptr);
    for (size_t i = 0; i < tf_hoppers->size(); i++) {
      assert((*tf_hoppers)[i] != nullptr);
      tf_hoppers_.emplace_back(std::move((*tf_hoppers)[i]));
      weights_.push_back(weights[i]);
    }
    content_hopper_ = std::move(*content_hopper);
  };
  virtual ~TfHopper(){};
  TfHopper(const TfHopper &) = delete;
  TfHopper &operator=(const TfHopper &) = delete;
  TfHopper(TfHopper &&) = delete;
  TfHopper &operator=(TfHopper &&) = delete;

private:
  fval epsilon_ = 0.0001;
  void tau_(addr k, addr *p, addr *q, fval *v) final {
    std::vector<addr> ps, tfs;
    addr min_p = maxfinity;
    for (size_t i = 0; i < tf_hoppers_.size(); i++) {
      addr p0, q0, tf0;
      tf_hoppers_[i]->tau(k, &p0, &q0, &tf0);
      ps.push_back(p0);
      tfs.push_back(tf0);
      min_p = std::min(min_p, p0);
    }
    fval tf = epsilon_;
    for (size_t i = 0; i < tfs.size(); i++)
      if (ps[i] == min_p)
        tf += weights_[i] * tfs[i];
    content_hopper_->tau(min_p, p, q);
    *v = tf;
  };
  void rho_(addr k, addr *p, addr *q, fval *v) final {
    addr p0, q0;
    content_hopper_->rho(k, &p0, &q0);
    tau(p0, p, q, v);
  };
  void uat_(addr k, addr *p, addr *q, fval *v) final {
    std::vector<addr> ps, tfs;
    addr max_p = minfinity;
    for (size_t i = 0; i < tf_hoppers_.size(); i++) {
      addr p0, q0, tf0;
      tf_hoppers_[i]->uat(k, &p0, &q0, &tf0);
      ps.push_back(p0);
      tfs.push_back(tf0);
      max_p = std::max(max_p, p0);
    }
    fval tf = epsilon_;
    for (size_t i = 0; i < tfs.size(); i++)
      if (ps[i] == max_p)
        tf += weights_[i] * tfs[i];
    content_hopper_->uat(max_p, p, q);
    *v = tf;
  };
  void ohr_(addr k, addr *p, addr *q, fval *v) final {
    addr p0, q0;
    content_hopper_->ohr(k, &p0, &q0);
    uat(q0, p, q, v);
  };
  std::vector<fval> weights_;
  std::vector<std::unique_ptr<Hopper>> tf_hoppers_;
  std::unique_ptr<Hopper> content_hopper_;
};
} // namespace

std::unique_ptr<Hopper> FieldStats::tf_hopper_(const std::string &term) {
  std::vector<std::unique_ptr<Hopper>> tf_hoppers;
  for (size_t i = 0; i < tf_featurizers_.size(); i++)
    tf_hoppers.emplace_back(warren_->idx()->hopper(tf_featurizers_[i]->featurize(term)));
  std::unique_ptr<Hopper> chopper = content_hopper(warren_);
  return std::make_unique<TfHopper>(&tf_hoppers, weights_, &chopper);
};

} // namespace cottontail
