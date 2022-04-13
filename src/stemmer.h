#ifndef COTTONTAIL_SRC_STEMMER_H_
#define COTTONTAIL_SRC_STEMMER_H_

#include <memory>
#include <string>

#include "src/core.h"

namespace cottontail {

class Stemmer {
public:
  static std::shared_ptr<Stemmer> make(const std::string &name,
                                       const std::string &recipe,
                                       std::string *error = nullptr);
  static bool check(const std::string &name, const std::string &recipe,
                    std::string *error = nullptr);
  std::string stem(const std::string &word, bool *stemmed = nullptr) {
    return stem_(word, stemmed);
  }
  inline std::string recipe() { return recipe_(); }
  inline std::string name() { return name_; }
  virtual ~Stemmer(){};
  Stemmer(const Stemmer &) = default;
  Stemmer &operator=(const Stemmer &) = default;

protected:
  Stemmer(){};

private:
  virtual std::string recipe_() = 0;
  virtual std::string stem_(const std::string &word, bool *stemmed) = 0;
  std::string name_ = "";
};

class NullStemmer final : public Stemmer {
public:
  NullStemmer(){};

private:
  std::string recipe_() final {
    return "";
  }
  std::string stem_(const std::string &word, bool *stemmed) final {
    if (stemmed != nullptr)
      *stemmed = false;
    return word;
  }
};
} // namespace cottontail
#endif // COTTONTAIL_SRC_STEMMER_H_
