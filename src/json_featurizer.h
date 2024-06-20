#ifndef COTTONTAIL_SRC_JSON_FEATURIZER_H_
#define COTTONTAIL_SRC_JSON_FEATURIZER_H_

#include <map>
#include <memory>

#include "src/core.h"
#include "src/wrapping_featurizer.h"

namespace cottontail {
class JsonFeaturizer : public WrappingFeaturizer {
public:
  static std::shared_ptr<Featurizer> make(std::shared_ptr<Featurizer> wrapped);

  JsonFeaturizer() = delete;
  virtual ~JsonFeaturizer(){};
  JsonFeaturizer(const JsonFeaturizer &) = delete;
  JsonFeaturizer &operator=(const JsonFeaturizer &) = delete;
  JsonFeaturizer(JsonFeaturizer &&) = delete;
  JsonFeaturizer &operator=(JsonFeaturizer &&) = delete;

private:
  JsonFeaturizer(std::shared_ptr<Featurizer> wrapped)
      : WrappingFeaturizer(wrapped){};
  addr featurize_(const char *key, addr length) final;
};
} // namespace cottontail
#endif // COTTONTAIL_SRC_JSON_FEATURIZER_H_
