#ifndef COTTONTAIL_SRC_TAGGING_FEATURIZER_H_
#define COTTONTAIL_SRC_TAGGING_FEATURIZER_H_

#include <memory>

#include "src/core.h"
#include "src/featurizer.h"

namespace cottontail {

class TaggingFeaturizer : public Featurizer {
public:
  static std::shared_ptr<Featurizer>
  make(const std::string &recipe, std::string *error = nullptr,
       std::shared_ptr<Working> working = nullptr);
  static std::shared_ptr<Featurizer>
  make(std::shared_ptr<Featurizer> base, std::string tag,
       std::string *error = nullptr);
  static bool check(const std::string &recipe, std::string *error = nullptr);

  TaggingFeaturizer(std::shared_ptr<Featurizer> base, std::string tag)
      : base_(base), tag_(tag + ":"){};

private:
  std::string recipe_() final;
  addr featurize_(const char *key, addr length) final {
    std::string key_string(key, length);
    return base_->featurize(tag_ + key_string);
  }
  std::string translate_(addr feature) final {
    return base_->translate(feature);
  }

  std::shared_ptr<Featurizer> base_;
  std::string tag_;
};
} // namespace cottontail
#endif // COTTONTAIL_SRC_TAGGING_FEATURIZER_H_
