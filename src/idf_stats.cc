#include "src/idf_stats.h"

#include <cassert>
#include <memory>
#include <string>

#include "src/core.h"
#include "src/hopper.h"
#include "src/tagging_featurizer.h"
#include "src/warren.h"

namespace cottontail {

std::shared_ptr<Stats> IdfStats::make(const std::string &recipe,
                                      std::shared_ptr<Warren> warren,
                                      std::string *error) {
  if (!check(recipe, error))
    return nullptr;
  std::shared_ptr<IdfStats> stats =
      std::shared_ptr<IdfStats>(new IdfStats(warren));
  TaggingFeaturizer featurizer(warren->featurizer(), "avgl");
  std::unique_ptr<Hopper> hopper =
      warren->idx()->hopper(featurizer.featurize("avgl"));
  if (hopper == nullptr) {
    safe_set(error) = "No global statistics in warren";
    return nullptr;
  }
  fval average_length;
  addr p, q;
  hopper->tau(minfinity + 1, &p, &q, &average_length);
  if (p == maxfinity) {
    safe_set(error) = "No global statistics in warren";
    return nullptr;
  }
  stats->average_length_ = average_length;
  stats->idf_featurizer_ =
      TaggingFeaturizer::make(warren->featurizer(), "idf", error);
  if (stats->idf_featurizer_ == nullptr)
    return nullptr;
  stats->rsj_featurizer_ =
      TaggingFeaturizer::make(warren->featurizer(), "rsj", error);
  if (stats->rsj_featurizer_ == nullptr)
    return nullptr;
  stats->tf_featurizer_ =
      TaggingFeaturizer::make(warren->featurizer(), "tf", error);
  if (stats->tf_featurizer_ == nullptr)
    return nullptr;
  std::string value;
  std::string unstemmed = "unstemmed";
  if (!warren->get_parameter(unstemmed, &value, error))
    stats->have_unstemmed_ = okay(value);
  std::string idf = "idf";
  if (!warren->get_parameter(idf, &value, error))
    stats->have_idf_ = okay(value);
  std::string rsj = "rsj";
  if (!warren->get_parameter(rsj, &value, error))
    stats->have_rsj_ = okay(value);
  return stats;
}

bool IdfStats::check(const std::string &recipe, std::string *error) {
  if (recipe != "") {
    safe_set(error) = "Bad recipe for IdfStats: " + recipe;
    return false;
  } else {
    return true;
  }
}

std::string IdfStats::recipe_() { return ""; }

bool IdfStats::have_(const std::string &name) {
  if (name == "avgl" || name == "tf")
    return true;
  if (name == "unstemmed")
    return have_unstemmed_;
  if (name == "idf")
    return have_idf_;
  if (name == "rsj")
    return have_rsj_;
  return false;
}

fval IdfStats::avgl_() { return average_length_; }

fval IdfStats::rsj_(const std::string &term) {
  fval rsj;
  addr p, q;
  std::unique_ptr<Hopper> hopper =
      warren_->idx()->hopper(rsj_featurizer_->featurize(term));
  if (hopper == nullptr)
    return 0.0;
  hopper->tau(minfinity + 1, &p, &q, &rsj);
  if (p == maxfinity)
    return 0.0;
  else
    return rsj;
}

fval IdfStats::idf_(const std::string &term) {
  fval idf;
  addr p, q;
  std::unique_ptr<Hopper> hopper =
      warren_->idx()->hopper(idf_featurizer_->featurize(term));
  if (hopper == nullptr)
    return 0.0;
  hopper->tau(minfinity + 1, &p, &q, &idf);
  if (p == maxfinity)
    return 0.0;
  else
    return idf;
}

std::unique_ptr<Hopper> IdfStats::tf_hopper_(const std::string &term) {
  return warren_->idx()->hopper(tf_featurizer_->featurize(term));
}

} // namespace cottontail
