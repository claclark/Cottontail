#ifndef COTTONTAIL_SRC_NULL_FEATURIZER_H_
#define COTTONTAIL_SRC_NULL_FEATURIZER_H_

#include <memory>
#include <string>

#include "src/core.h"
#include "src/working.h"

namespace cottontail {

class NullFeaturizer : public Featurizer {
public:
  static std::shared_ptr<Featurizer> make(const std::string &recipe,
                                          std::string *error = nullptr) {
    return std::shared_ptr<NullFeaturizer>(new NullFeaturizer());
  };
  static bool check(const std::string &recipe, std::string *error = nullptr) {
    return true;
  };

  virtual ~NullFeaturizer(){};
  NullFeaturizer(const NullFeaturizer &) = delete;
  NullFeaturizer &operator=(const NullFeaturizer &) = delete;
  NullFeaturizer(NullFeaturizer &&) = delete;
  NullFeaturizer &operator=(NullFeaturizer &&) = delete;

private:
  NullFeaturizer(){};
  virtual std::string recipe_() final { return ""; }
  virtual addr featurize_(const char *key, addr length) final { return 0; };
  virtual std::string translate_(addr feature) final { return ""; };
};

} // namespace cottontail
#endif // COTTONTAIL_SRC_NULL_FEATURIZER_H_
