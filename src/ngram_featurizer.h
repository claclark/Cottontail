#ifndef COTTONTAIL_SRC_NGRAM_FEATURIZER_H_
#define COTTONTAIL_SRC_NGRAM_FEATURIZER_H_

#include <memory>
#include <string>

#include "src/core.h"
#include "src/featurizer.h"

namespace cottontail {

// Unicode noncharacters...
const std::string ngram_marker = "\xEF\xB7\x9A";
const std::string universal_marker = "\xEF\xB7\x9B";
const std::string translate_marker = "\xEF\xB7\x9C";

class NGramFeaturizer final : public Featurizer {
public:
  NGramFeaturizer(){};
  static std::shared_ptr<Featurizer> make(const std::string &recipe,
                                          std::string *error = nullptr);
  static bool check(const std::string &recipe, std::string *error = nullptr);

  static std::shared_ptr<Featurizer> make() {
    std::string error;
    return make("", &error);
  }

  virtual ~NGramFeaturizer(){};
  NGramFeaturizer(const NGramFeaturizer &) = delete;
  NGramFeaturizer &operator=(const NGramFeaturizer &) = delete;
  NGramFeaturizer(NGramFeaturizer &&) = delete;
  NGramFeaturizer &operator=(NGramFeaturizer &&) = delete;

private:
  std::string name_() final { return "ngram"; }
  std::string recipe_() final { return ""; }
  addr featurize_(const char *key, addr length) final;
  std::string translate_(addr feature) final;
};

} // namespace cottontail

#endif // COTTONTAIL_SRC_NGRAM_FEATURIZER_H_
