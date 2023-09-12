#include "src/simple_warren.h"

#include <iostream>
#include <map>

#include "src/annotator.h"
#include "src/appender.h"
#include "src/core.h"
#include "src/fastid_txt.h"
#include "src/featurizer.h"
#include "src/recipe.h"
#include "src/simple.h"
#include "src/stemmer.h"
#include "src/tokenizer.h"

namespace cottontail {

std::shared_ptr<Warren> SimpleWarren::make(const std::string &burrow,
                                           std::string *error) {
  std::string the_burrow = burrow;
  if (the_burrow == "")
    the_burrow = DEFAULT_BURROW;
  std::shared_ptr<Working> working = Working::make(the_burrow, error);
  if (working == nullptr)
    return nullptr;
  std::string dna;
  if (!read_dna(working, &dna, error))
    return nullptr;
  std::map<std::string, std::string> parameters;
  if (!cook(dna, &parameters, error))
    return nullptr;
  std::map<std::string, std::string> featurizer_parameters;
  if (!cook(parameters["featurizer"], &featurizer_parameters, error))
    return nullptr;
  std::shared_ptr<Featurizer> featurizer =
      Featurizer::make(featurizer_parameters["name"],
                       featurizer_parameters["recipe"], error, working);
  if (featurizer == nullptr)
    return nullptr;
  std::map<std::string, std::string> tokenizer_parameters;
  if (!cook(parameters["tokenizer"], &tokenizer_parameters, error))
    return nullptr;
  std::shared_ptr<Tokenizer> tokenizer = Tokenizer::make(
      tokenizer_parameters["name"], tokenizer_parameters["recipe"], error);
  if (tokenizer == nullptr)
    return nullptr;
  std::map<std::string, std::string> txt_parameters;
  if (!cook(parameters["txt"], &txt_parameters, error))
    return nullptr;
  std::shared_ptr<Txt> txt =
      Txt::make(txt_parameters["name"], txt_parameters["recipe"], error,
                tokenizer, working);
  if (txt == nullptr)
    return nullptr;
  std::map<std::string, std::string> idx_parameters;
  if (!cook(parameters["idx"], &idx_parameters, error))
    return nullptr;
  bool preload = false;
  std::string container_query;
  std::string stemmer_name, stemmer_recipe;
  if (parameters.find("parameters") != parameters.end()) {
    std::map<std::string, std::string> extra_parameters;
    if (!cook(parameters["parameters"], &extra_parameters, error))
      return nullptr;
    auto container_element = extra_parameters.find("container");
    if (container_element != extra_parameters.end())
      container_query = container_element->second;
    auto stemmer_element = extra_parameters.find("stemmer");
    if (stemmer_element != extra_parameters.end())
      stemmer_name = stemmer_element->second;
    std::string fastid;
    auto fastid_element = extra_parameters.find("fastid");
    if (fastid_element != extra_parameters.end())
      fastid = fastid_element->second;
    if (okay(fastid)) {
      txt = FastidTxt::make(txt, working, error);
      if (txt == nullptr)
        return nullptr;
    }
    auto preload_element = extra_parameters.find("preload");
    if (preload_element != extra_parameters.end()) {
      std::string preload_value = preload_element->second;
      preload = okay(preload_value);
    }
  }
  if (preload)
    working->load(PST_NAME);
  std::shared_ptr<Idx> idx = Idx::make(
      idx_parameters["name"], idx_parameters["recipe"], error, working);
  if (idx == nullptr)
    return nullptr;
  std::shared_ptr<SimpleWarren> warren = std::shared_ptr<SimpleWarren>(
      new SimpleWarren(working, featurizer, tokenizer, idx, txt));
  warren->annotator_ = Annotator::make(
      idx_parameters["name"], idx_parameters["recipe"], error, working);
  if (warren->annotator_ == nullptr)
    return nullptr;
  warren->appender_ =
      Appender::make(txt_parameters["name"], txt_parameters["recipe"], error,
                     working, featurizer, tokenizer, warren->annotator_);
  if (warren->appender_ == nullptr)
    return nullptr;
  if (container_query != "")
    warren->default_container_ = container_query;
  if (stemmer_name != "") {
    std::shared_ptr<Stemmer> stemmer =
        Stemmer::make(stemmer_name, stemmer_recipe, error);
    if (stemmer == nullptr)
      return nullptr;
    warren->stemmer_ = stemmer;
  }
  return warren;
}

bool SimpleWarren::set_parameter_(const std::string &key,
                                  const std::string &value,
                                  std::string *error) {
  return set_parameter_in_dna(working(), key, value, error);
}

bool SimpleWarren::get_parameter_(const std::string &key, std::string *value,
                                  std::string *error) {
  return get_parameter_from_dna(working(), key, value, error);
}

bool SimpleWarren::transaction_(std::string *error) {
  if (!appender_->transaction(error))
    return false;
  if (!annotator()->transaction(error)) {
    appender_->abort();
    return false;
  }
  return true;
}

bool SimpleWarren::ready_() {
  return appender_->ready() && annotator_->ready();
}

void SimpleWarren::commit_() {
  appender_->commit();
  annotator_->commit();
}

void SimpleWarren::abort_() {
  appender_->abort();
  annotator_->abort();
}
} // namespace cottontail
