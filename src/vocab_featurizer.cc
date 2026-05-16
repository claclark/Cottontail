#include "src/vocab_featurizer.h"

#include <fstream>
#include <memory>
#include <string>

#include "src/core.h"
#include "src/featurizer.h"
#include "src/recipe.h"

namespace cottontail {

namespace {

const std::string DEFAULT_WRAPPED_NAME = "hashing";
const std::string DEFAULT_WRAPPED_RECIPE = "";

bool interpret_recipe(const std::string &recipe, std::string *wrapped_name,
                      std::string *wrapped_recipe, std::string *error) {
  if (recipe == "") {
    *wrapped_name = DEFAULT_WRAPPED_NAME;
    *wrapped_recipe = DEFAULT_WRAPPED_RECIPE;
    return true;
  }
  std::map<std::string, std::string> parameters;
  if (cook(recipe, &parameters, error)) {
    if (parameters.find("name") == parameters.end() ||
        parameters.find("recipe") == parameters.end()) {
      safe_error(error) = "VocabFeaturizer got a bad recipe";
      return false;
    } else {
      *wrapped_name = parameters["name"];
      *wrapped_recipe = parameters["recipe"];
    }
  } else {
    *wrapped_name = recipe;
    *wrapped_recipe = "";
  }
  return Featurizer::check(*wrapped_name, *wrapped_recipe, error);
}

} // namespace

std::shared_ptr<Featurizer>
VocabFeaturizer::make(const std::string &recipe,
                      std::shared_ptr<Working> working, std::string *error) {
  if (working == nullptr) {
    safe_error(error) = "VocabFeaturizer requires a working directory";
    return nullptr;
  }
  std::string wrapped_name;
  std::string wrapped_recipe;
  if (!interpret_recipe(recipe, &wrapped_name, &wrapped_recipe, error))
    return nullptr;
  std::shared_ptr<Featurizer> wrapped_featurizer =
      Featurizer::make(wrapped_name, wrapped_recipe, error, working);
  if (wrapped_featurizer == nullptr)
    return nullptr;
  std::string vocab_filename = working->make_name(VOCAB_NAME);
  std::fstream vocabf;
  vocabf.open(vocab_filename, std::ios::in);
  if (!vocabf.fail()) {
    // We already have a vocabulary.
    vocabf.close();
    return wrapped_featurizer;
  }
  std::shared_ptr<VocabFeaturizer> featurizer =
      std::shared_ptr<VocabFeaturizer>(new VocabFeaturizer());
  featurizer->wrapped_featurizer_ = wrapped_featurizer;
  featurizer->working_ = working;
  return featurizer;
}

bool VocabFeaturizer::check(const std::string &recipe, std::string *error) {
  std::string wrapped_name;
  std::string wrapped_recipe;
  return interpret_recipe(recipe, &wrapped_name, &wrapped_recipe, error);
}

VocabFeaturizer::~VocabFeaturizer() {
  if (working_ != nullptr) {
    std::string vocab_filename = working_->make_name(VOCAB_NAME);
    std::fstream vocabf;
    vocabf.open(vocab_filename, std::ios::out);
    if (!vocabf.fail()) {
      for (auto &term : vocab_) {
        addr feature = wrapped_featurizer_->featurize(term.first);
        vocabf << feature << " " << term.second << " " << term.first << "\n";
      }
      vocabf.close();
    }
  }
}

std::string VocabFeaturizer::name_() { return wrapped_featurizer_->name(); }

std::string VocabFeaturizer::recipe_() {
  return wrapped_featurizer_->recipe();
}

addr VocabFeaturizer::featurize_(const char *key, addr length) {
  std::string term(key, key + length);
  if (vocab_.find(term) == vocab_.end())
    vocab_[term] = 1;
  else
    vocab_[term]++;
  return wrapped_featurizer_->featurize(key, length);
}

std::string VocabFeaturizer::translate_(addr feature) {
  return wrapped_featurizer_->translate(feature);
}

} // namespace cottontail
