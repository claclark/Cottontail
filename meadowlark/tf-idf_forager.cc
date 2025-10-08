#include "meadowlark/tf-idf_forager.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "src/core.h"
#include "src/tagging_featurizer.h"
#include "src/warren.h"

namespace cottontail {
namespace meadowlark {

std::shared_ptr<Forager>
TfIdfForager::make(std::shared_ptr<Warren> warren, const std::string &tag,
                   const std::map<std::string, std::string> &parameters,
                   std::string *error) {
  std::shared_ptr<TfIdfForager> forager =
      std::shared_ptr<TfIdfForager>(new TfIdfForager());
  forager->tag_ = tag;
  if (parameters.find("stemmer") == parameters.end())
    forager->stemmer_ = Stemmer::make("porter", "", error);
  else
    forager->stemmer_ = Stemmer::make(parameters.at("stemmer"), "", error);
  if (forager->stemmer_ == nullptr)
    return nullptr;
  if (parameters.find("tokenizer") == parameters.end())
    forager->tokenizer_ = Tokenizer::make("ascii", "", error);
  else
    forager->tokenizer_ =
        Tokenizer::make(parameters.at("tokenizer"), "", error);
  if (forager->tokenizer_ == nullptr)
    return nullptr;
  forager->df_featurizer_ =
      TaggingFeaturizer::make(warren->featurizer(), tag + "df", error);
  if (forager->df_featurizer_ == nullptr)
    return nullptr;
  forager->tf_featurizer_ =
      TaggingFeaturizer::make(warren->featurizer(), tag + "tf", error);
  if (forager->tf_featurizer_ == nullptr)
    return nullptr;
  forager->tf_featurizer_ =
      TaggingFeaturizer::make(warren->featurizer(), tag + "total", error);
  if (forager->tf_featurizer_ == nullptr)
    return nullptr;
  return forager;
}

bool TfIdfForager::check(const std::string &tag,
                         const std::map<std::string, std::string> &parameters,
                         std::string *error) {
  return true;
}

bool TfIdfForager::forage_(addr p, addr q, std::string *error) {
  if (p > q || p == minfinity || q == maxfinity) {
    safe_error(error) = "Invalid range.";
    return false;
  }
  p_min_ = std::min(p_min_, p);
  total_items_++;
  total_length_ += q - p + 1;
  std::map<std::string, addr> tf;
  std::string text = warren_->txt()->translate(p, q);
  std::vector<std::string> tokens = tokenizer_->split(text);
  for (auto &token : tokens) {
    std::string stem;
    {
      auto it = stems_.find(token);
      if (it == stems_.end())
        stem = stems_[token] = stemmer_->stem(token);
      else
        stem = it->second;
    }
    {
      auto it = tf.find(stem);
      if (it == tf.end())
        tf[stem] = 1;
      else
        it->second++;
    }
  }
  for (auto &token : tf) {
    addr tf_feature = tf_featurizer_->featurize(token.first);
    if (!warren_->annotator()->annotate(tf_feature, p, p, token.second, error))
      return false;
    addr df_feature = df_featurizer_->featurize(token.first);
    {
      auto it = df_.find(df_feature);
      if (it == df_.end())
        df_[df_feature] = 1;
      else
        it->second++;
    }
  }
  return true;
}

bool TfIdfForager::ready_() {
  if (p_min_ == maxfinity)
    return true;
  for (auto &feature : df_)
    if (!warren_->annotator()->annotate(feature.first, p_min_, p_min_,
                                        feature.second))
      return false;
  if (!warren_->annotator()->annotate(total_featurizer_->featurize("items"),
                                      p_min_, p_min_, total_items_))
    return false;
  if (!warren_->annotator()->annotate(total_featurizer_->featurize("length"),
                                      p_min_, p_min_, total_length_))
    return false;
  return true;
}

} // namespace meadowlark
} // namespace cottontail
