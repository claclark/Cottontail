#include "src/warren.h"

#include "src/core.h"
#include "src/recipe.h"
#include "src/simple_warren.h"
#include "src/stats.h"

namespace cottontail {
std::shared_ptr<Warren> Warren::make(const std::string &name,
                                     const std::string &burrow,
                                     std::string *error) {
  std::shared_ptr<Warren> warren;
  if (name == "" || name == "simple") {
    warren = SimpleWarren::make(burrow, error);
    if (warren == nullptr)
      return nullptr;
  } else {
    safe_set(error) = "No Warren named: " + name;
    return nullptr;
  }
  warren->start();
  warren->name_ = name;
  std::string container_key = "container";
  std::string container_value;
  if (!warren->get_parameter(container_key, &container_value, error))
    return nullptr;
  warren->default_container_ = container_value;
  std::string stemmer_key = "stemmer";
  std::string stemmer_value;
  if (!warren->get_parameter(stemmer_key, &stemmer_value, error))
    return nullptr;
  if (stemmer_value != "") {
    std::string empty_recipe = "";
    std::shared_ptr<Stemmer> stemmer =
        Stemmer::make(stemmer_value, empty_recipe, error);
    if (stemmer == nullptr)
      return nullptr;
    warren->stemmer_ = stemmer;
  }
  warren->end();
  return warren;
}

std::string Warren::base_dna() {
  std::map<std::string, std::string> warren_parameters;
  warren_parameters["warren"] = name();
  if (tokenizer() != nullptr) {
    std::map<std::string, std::string> tokenizer_parameters;
    tokenizer_parameters["name"] = tokenizer_->name();
    tokenizer_parameters["recipe"] = tokenizer_->recipe();
    warren_parameters["tokenizer"] = freeze(tokenizer_parameters);
  }
  if (featurizer() != nullptr) {
    std::map<std::string, std::string> featurizer_parameters;
    featurizer_parameters["name"] = featurizer()->name();
    featurizer_parameters["recipe"] = featurizer()->recipe();
    warren_parameters["featurizer"] = freeze(featurizer_parameters);
  }
  if (txt() != nullptr) {
    std::map<std::string, std::string> txt_parameters;
    txt_parameters["name"] = txt()->name();
    txt_parameters["recipe"] = txt()->recipe();
    warren_parameters["txt"] = freeze(txt_parameters);
  }
  if (idx() != nullptr) {
    std::map<std::string, std::string> idx_parameters;
    idx_parameters["name"] = idx()->name();
    idx_parameters["recipe"] = idx()->recipe();
    warren_parameters["idx"] = freeze(idx_parameters);
  }
  return freeze(warren_parameters);
}
} // namespace cottontail
