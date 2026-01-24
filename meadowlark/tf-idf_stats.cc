#include "meadowlark/tf-idf_stats.h"

#include <cassert>
#include <cmath>
#include <memory>
#include <string>

#include "meadowlark/forager.h"
#include "src/core.h"
#include "src/hopper.h"
#include "src/tagging_featurizer.h"
#include "src/tf_hopper.h"
#include "src/warren.h"

namespace cottontail {
namespace meadowlark {

std::shared_ptr<Stats> TfIdfStats::make(const std::string &recipe,
                                        std::shared_ptr<Warren> warren,
                                        std::string *error) {
  std::string label = forager_label("tf-idf", recipe);
  std::shared_ptr<Hopper> hopper =
      warren->idx()->hopper(warren->featurizer()->featurize("@" + label));
  if (hopper == nullptr) {
    if (recipe == "")
      safe_error(error) = "No tf-idf stats in meadow";
    else
      safe_error(error) = "No tf-idf stats in meadow for tag: " + recipe;
    return nullptr;
  }
  addr p, q;
  hopper->tau(minfinity + 1, &p, &q);
  if (p == maxfinity) {
    if (recipe == "")
      safe_error(error) = "No tf-idf stats in meadow";
    else
      safe_error(error) = "No tf-idf stats in meadow for tag: " + recipe;
    return nullptr;
  }
  std::string name, tag;
  std::map<std::string, std::string> parameters;
  if (!json2forager(warren->txt()->translate(p, q), &name, &tag, &parameters,
                    error))
    return nullptr;
  if (name != "tf-idf" || tag != recipe ||
      parameters.find("gcl") == parameters.end()) {
    safe_error(error) = "Metadata inconsistency";
    return nullptr;
  }
  std::shared_ptr<TfIdfStats> stats =
      std::shared_ptr<TfIdfStats>(new TfIdfStats(warren));
  stats->tag_ = recipe;
  stats->label_ = label + ":";
  stats->content_query_ = parameters["gcl"];
  if ((hopper = warren->hopper_from_gcl(stats->content_query_, error)) ==
      nullptr)
    return nullptr;
  if (parameters.find("container") == parameters.end())
    stats->container_query_ = ":";
  else
    stats->container_query_ = parameters["container"];
  if ((hopper = warren->hopper_from_gcl(stats->container_query_, error)) ==
      nullptr)
    return nullptr;
  if (parameters.find("stemmer") == parameters.end())
    stats->stemmer_ = Stemmer::make("porter", "", error);
  else
    stats->stemmer_ = Stemmer::make(parameters["stemmer"], "", error);
  if (stats->stemmer_ == nullptr)
    return nullptr;
  if (parameters.find("tokenizer") == parameters.end())
    stats->tokenizer_ = Tokenizer::make("ascii", "", error);
  else
    stats->tokenizer_ = Tokenizer::make(parameters["tokenizer"], "", error);
  if (stats->tokenizer_ == nullptr)
    return nullptr;
  stats->tf_featurizer_ =
      TaggingFeaturizer::make(warren->featurizer(), label + "tf", error);
  if (stats->tf_featurizer_ == nullptr)
    return nullptr;
  std::shared_ptr<Featurizer> total_featurizer =
      TaggingFeaturizer::make(warren->featurizer(), label + "total", error);
  hopper = warren->idx()->hopper(total_featurizer->featurize("items"));
  if (hopper == nullptr) {
    if (recipe == "")
      safe_error(error) = "No tf-idf stats in meadow";
    else
      safe_error(error) = "No tf-idf stats in meadow for tag: " + recipe;
    return nullptr;
  }
  addr n, items = 0;
  for (hopper->tau(minfinity + 1, &p, &q, &n); p < maxfinity;
       hopper->tau(p + 1, &p, &q, &n))
    items += n;
  if (items < 1) {
    if (recipe == "")
      safe_error(error) = "No tf-idf stats in meadow";
    else
      safe_error(error) = "No tf-idf stats in meadow for tag: " + recipe;
    return nullptr;
  }
  stats->items_ = items;
  hopper = warren->idx()->hopper(total_featurizer->featurize("length"));
  if (hopper == nullptr) {
    if (recipe == "")
      safe_error(error) = "No tf-idf stats in meadow";
    else
      safe_error(error) = "No tf-idf stats in meadow for tag: " + recipe;
    return nullptr;
  }
  addr length = 0;
  for (hopper->tau(minfinity + 1, &p, &q, &n); p < maxfinity;
       hopper->tau(p + 1, &p, &q, &n))
    length += n;
  if (length < 1) {
    if (recipe == "")
      safe_error(error) = "No tf-idf stats in meadow";
    else
      safe_error(error) = "No tf-idf stats in meadow for tag: " + recipe;
    return nullptr;
  }
  stats->average_length_ = length / stats->items_;
  stats->tf_featurizer_ =
      TaggingFeaturizer::make(warren->featurizer(), label + "tf", error);
  if (stats->tf_featurizer_ == nullptr)
    return nullptr;
  return stats;
}

bool TfIdfStats::check(const std::string &recipe, std::string *error) {
  return true;
}

std::string TfIdfStats::recipe_() { return tag_; }

bool TfIdfStats::have_(const std::string &name) {
  return name == "content" || name == "avgl" || name == "rsj" ||
         name == "idf" || name == "tf";
}

fval TfIdfStats::avgl_() { return average_length_; }

namespace {
inline addr load_df(std::shared_ptr<Warren> warren,
                    std::shared_ptr<Featurizer> featurizer,
                    const std::string &term) {
  return warren->idx()->count(featurizer->featurize(term));
}
} // namespace

fval TfIdfStats::idf_(const std::string &term) {
  fval df = (fval)load_df(warren_, tf_featurizer_, term);
  if (df == 0.0)
    return 0.0;
  fval idf = std::log(items_ / df);
  if (idf < 0.0)
    return 0.0;
  return idf;
}

fval TfIdfStats::rsj_(const std::string &term) {
  fval df = (fval)load_df(warren_, tf_featurizer_, term);
  if (df == 0.0)
    return 0.0;
  fval rsj = std::log((items_ - df + 0.5) / (df + 0.5));
  if (rsj < 0.0)
    return 0.0;
  return rsj;
}

std::unique_ptr<Hopper> TfIdfStats::tf_hopper_(const std::string &term) {
  std::unique_ptr<Hopper> tf_hopper =
      warren_->idx()->hopper(tf_featurizer_->featurize(term));
  assert(tf_hopper != nullptr);
  std::unique_ptr<Hopper> chopper = content_hopper(warren_);
  assert(chopper != nullptr);
  return std::make_unique<TfHopper>(std::move(tf_hopper), std::move(chopper));
}

std::unique_ptr<Hopper> TfIdfStats::container_hopper_() {
  std::unique_ptr<Hopper> hopper = warren_->hopper_from_gcl(container_query_);
  assert(hopper != nullptr);
  return hopper;
}
} // namespace meadowlark
} // namespace cottontail
