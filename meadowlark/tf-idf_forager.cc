#include "meadowlark/tf-idf_forager.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "src/annotator.h"
#include "src/core.h"
#include "src/tagging_featurizer.h"
#include "src/tokenizer.h"
#include "src/warren.h"

namespace cottontail {
namespace meadowlark {

std::shared_ptr<Forager> TfIdfForager::make(
    std::shared_ptr<Featurizer> featurizer, const std::string &tag,
    const std::map<std::string, std::string> &parameters, std::string *error) {
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
    forager->tokenizer_ = Tokenizer::make(parameters.at("tokenizer"), "", error);
  if (forager->tokenizer_ == nullptr)
    return nullptr;
  forager->df_featurizer_ =
      TaggingFeaturizer::make(featurizer, tag + "df", error);
  if (forager->df_featurizer_ == nullptr)
    return nullptr;
  forager->tf_featurizer_ =
      TaggingFeaturizer::make(featurizer, tag + "tf", error);
  if (forager->tf_featurizer_ == nullptr)
    return nullptr;
  forager->tf_featurizer_ =
      TaggingFeaturizer::make(featurizer, tag + "total", error);
  if (forager->tf_featurizer_ == nullptr)
    return nullptr;
  return forager;
}

bool TfIdfForager::forage_(std::shared_ptr<Forager> annotator,
                           const std::string &text,
                           const std::vector<Token> &tokens,
                           std::string *error) {
  return true;
}

} // namespace meadowlark
} // namespace cottontail
