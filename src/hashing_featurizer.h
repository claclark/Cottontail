#ifndef COTTONTAIL_SRC_HASHING_FEATURIZER_H_
#define COTTONTAIL_SRC_HASHING_FEATURIZER_H_

#include <memory>

#include "src/core.h"
#include "src/featurizer.h"

namespace cottontail {

class HashingFeaturizer : public Featurizer {
public:
  HashingFeaturizer(){};
  static std::shared_ptr<Featurizer> make(const std::string &recipe,
                                          std::string *error = nullptr);
  static bool check(const std::string &recipe, std::string *error = nullptr);

  static std::shared_ptr<Featurizer> make() {
    std::string empty = "";
    std::string error;
    return make(empty, &error);
  }

  virtual ~HashingFeaturizer(){};
  HashingFeaturizer(const HashingFeaturizer &) = delete;
  HashingFeaturizer &operator=(const HashingFeaturizer &) = delete;
  HashingFeaturizer(HashingFeaturizer &&) = delete;
  HashingFeaturizer &operator=(HashingFeaturizer &&) = delete;

private:
  std::string recipe_() final;
  addr featurize_(const char *key, addr length) final;
  std::string translate_(addr feature) final;
};
} // namespace cottontail
#endif // COTTONTAIL_SRC_HASHING_FEATURIZER_H_
