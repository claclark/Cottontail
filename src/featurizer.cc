#include "src/featurizer.h"

#include <map>
#include <memory>
#include <string>

#include "src/core.h"
#include "src/hashing_featurizer.h"
#include "src/null_featurizer.h"
#include "src/tagging_featurizer.h"
#include "src/vocab_featurizer.h"

namespace cottontail {

std::shared_ptr<Featurizer> Featurizer::make(const std::string &name,
                                             const std::string &recipe,
                                             std::string *error,
                                             std::shared_ptr<Working> working) {
  std::shared_ptr<Featurizer> featurizer;
  if (name == "" || name == "hashing") {
    featurizer = HashingFeaturizer::make(recipe, error);
    if (featurizer != nullptr)
      featurizer->name_ = "hashing";
    return featurizer;
  } else if (name == "tagging") {
    featurizer = TaggingFeaturizer::make(recipe, error, working);
    if (featurizer != nullptr)
      featurizer->name_ = "tagging";
    return featurizer;
  } else if (name == "vocab") {
    featurizer = VocabFeaturizer::make(recipe, working, error);
    if (featurizer != nullptr)
      featurizer->name_ = "vocab";
    return featurizer;
  } else if (name == "null") {
    featurizer = NullFeaturizer::make(recipe, error);
    if (featurizer != nullptr)
      featurizer->name_ = "null";
    return featurizer;
  } else {
    safe_set(error) = "No Featurizer named: " + name;
    return nullptr;
  }
}

bool Featurizer::check(const std::string &name, const std::string &recipe,
                       std::string *error) {
  if (name == "" || name == "hashing") {
    return HashingFeaturizer::check(recipe, error);
  } else if (name == "tagging") {
    return TaggingFeaturizer::check(recipe, error);
  } else if (name == "vocab") {
    return VocabFeaturizer::check(recipe, error);
  } else {
    safe_set(error) = "No Featurizer named: " + name;
    return false;
  }
}
} // namespace cottontail
