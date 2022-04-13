#ifndef COTTONTAIL_SRC_VOCAB_FEATURIZER_H_
#define COTTONTAIL_SRC_VOCAB_FEATURIZER_H_

#include <map>
#include <memory>

#include "src/core.h"
#include "src/featurizer.h"
#include "src/working.h"

namespace cottontail {
static const std::string VOCAB_NAME = "vocabulary";

class VocabFeaturizer : public Featurizer {
public:
  static std::shared_ptr<Featurizer> make(const std::string &recipe,
                                          std::shared_ptr<Working> working,
                                          std::string *error = nullptr);
  static bool check(const std::string &recipe, std::string *error = nullptr);
  ~VocabFeaturizer() final;

private:
  VocabFeaturizer(){};
  std::string recipe_() final;
  addr featurize_(const char *key, addr length) final;
  std::string translate_(addr feature) final;
  std::shared_ptr<Featurizer> wrapped_featurizer_;
  std::map<std::string, size_t> vocab_;
  std::shared_ptr<Working> working_ = nullptr;
};
} // namespace cottontail
#endif // COTTONTAIL_SRC_VOCAB_FEATURIZER_H_
