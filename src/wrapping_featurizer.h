#ifndef COTTONTAIL_SRC_WRAPPING_FEATURIZER_H_
#define COTTONTAIL_SRC_WRAPPING_FEATURIZER_H_

#include <cassert>
#include <map>
#include <memory>

#include "src/core.h"
#include "src/featurizer.h"

namespace cottontail {
class WrappingFeaturizer : public Featurizer {
public:
  virtual ~WrappingFeaturizer(){};
  WrappingFeaturizer(const WrappingFeaturizer &) = delete;
  WrappingFeaturizer &operator=(const WrappingFeaturizer &) = delete;
  WrappingFeaturizer(WrappingFeaturizer &&) = delete;
  WrappingFeaturizer &operator=(WrappingFeaturizer &&) = delete;

protected:
  WrappingFeaturizer(std::shared_ptr<Featurizer> wrapped) : wrapped_(wrapped) {
    assert(wrapped_ != nullptr);
  };
  std::shared_ptr<Featurizer> wrapped_;

private:
  WrappingFeaturizer(){};
  std::string recipe_() final;
  addr featurize_(const char *key, addr length);
  std::string translate_(addr feature);
};
} // namespace cottontail
#endif // COTTONTAIL_SRC_WRAPPING_FEATURIZER_H_
