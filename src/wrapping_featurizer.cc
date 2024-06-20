#include "src/wrapping_featurizer.h"

#include <memory>
#include <string>

#include "src/core.h"
#include "src/featurizer.h"
#include "src/recipe.h"

namespace cottontail {
std::string WrappingFeaturizer::recipe_() {
  assert(wrapped_ != nullptr);
  std::string wrapped_name = wrapped_->name();
  if (wrapped_name == "")
    return "";
  std::string wrapped_recipe = wrapped_->recipe();
  if (wrapped_recipe == "")
    return wrapped_name;
  std::map<std::string, std::string> parameters;
  parameters["name"] = wrapped_name;
  parameters["recipe"] = wrapped_recipe;
  return freeze(parameters);
}

addr WrappingFeaturizer::featurize_(const char *key, addr length) {
  return wrapped_->featurize(key, length);
}

std::string WrappingFeaturizer::translate_(addr feature) {
  return wrapped_->translate(feature);
}
} // namespace cottontail
