#include "src/tagging_featurizer.h"

#include <limits>
#include <string>

#include "src/core.h"
#include "src/featurizer.h"
#include "src/recipe.h"

namespace cottontail {
std::shared_ptr<Featurizer>
TaggingFeaturizer::make(const std::string &recipe, std::string *error,
                        std::shared_ptr<Working> working) {
  std::map<std::string, std::string> base_parameters;
  if (!cook(recipe, &base_parameters)) {
    safe_set(error) = "TaggingFeaturizer got bad recipe";
    return nullptr;
  }
  std::string base_name;
  std::map<std::string, std::string>::iterator n_item =
      base_parameters.find("name");
  if (n_item != base_parameters.end()) {
    base_name = n_item->second;
  } else {
    safe_set(error) = "TaggingFeaturizer has no base Featurizer";
    return nullptr;
  }
  std::string base_recipe = "";
  std::map<std::string, std::string>::iterator r_item =
      base_parameters.find("recipe");
  if (r_item != base_parameters.end())
    base_recipe = r_item->second;
  std::string tag;
  std::map<std::string, std::string>::iterator t_item =
      base_parameters.find("tag");
  if (t_item != base_parameters.end()) {
    tag = t_item->second;
  } else {
    safe_set(error) = "TaggingFeaturizer has no tag";
    return nullptr;
  }
  std::shared_ptr<Featurizer> base_featurizer =
      Featurizer::make(base_name, base_recipe, error, working);
  return TaggingFeaturizer::make(base_featurizer, tag, error);
}

std::shared_ptr<Featurizer>
TaggingFeaturizer::make(std::shared_ptr<Featurizer> base, std::string tag,
                        std::string *error) {
  if (base == nullptr) {
    safe_set(error) = "TaggingFeaturizer needs a base Featurizer (got nullptr)";
    return nullptr;
  }
  return std::shared_ptr<TaggingFeaturizer>(new TaggingFeaturizer(base, tag));
}

bool TaggingFeaturizer::check(const std::string &recipe, std::string *error) {
  std::map<std::string, std::string> base_parameters;
  if (!cook(recipe, &base_parameters)) {
    safe_set(error) = "TaggingFeaturizer got bad recipe";
    return false;
  }
  std::string base_name;
  std::map<std::string, std::string>::iterator n_item =
      base_parameters.find("name");
  if (n_item != base_parameters.end()) {
    base_name = n_item->second;
  } else {
    safe_set(error) = "TaggingFeaturizer has no base Featurizer";
    return false;
  }
  std::string base_recipe = "";
  std::map<std::string, std::string>::iterator r_item =
      base_parameters.find("recipe");
  if (r_item != base_parameters.end())
    base_recipe = r_item->second;
  std::map<std::string, std::string>::iterator t_item =
      base_parameters.find("tag");
  if (t_item == base_parameters.end()) {
    safe_set(error) = "TaggingFeaturizer has no tag";
    return false;
  }
  return Featurizer::check(base_name, base_recipe, error);
}

std::string TaggingFeaturizer::recipe_() {
  std::map<std::string, std::string> parameters;
  parameters["tag"] = tag_;
  parameters["name"] = base_->name();
  parameters["recipe"] = base_->recipe();
  std::string recipe;
  return freeze(parameters);
}

} // namespace cottontail
