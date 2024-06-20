#include "src/json_featurizer.h"

#include <memory>
#include <string>

#include "src/core.h"
#include "src/json.h"
#include "src/recipe.h"

namespace cottontail {

std::shared_ptr<Featurizer>
JsonFeaturizer::make(std::shared_ptr<Featurizer> wrapped) {
  return std::shared_ptr<JsonFeaturizer>(new JsonFeaturizer(wrapped));
}

addr JsonFeaturizer::featurize_(const char *key, addr length) {
  if (json_internal_token(key, length))
    return null_feature;
  else
    return wrapped_->featurize(key, length);
}

} // namespace cottontail
