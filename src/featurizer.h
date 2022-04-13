#ifndef COTTONTAIL_SRC_FEATURIZER_H_
#define COTTONTAIL_SRC_FEATURIZER_H_

#include <memory>
#include <string>

#include "src/core.h"
#include "src/working.h"

namespace cottontail {

class Featurizer {
public:
  static std::shared_ptr<Featurizer>
  make(const std::string &name, const std::string &recipe,
       std::string *error = nullptr,
       std::shared_ptr<Working> working = nullptr);
  static bool check(const std::string &name, const std::string &recipe,
                    std::string *error = nullptr);
  inline std::string recipe() { return recipe_(); }
  inline std::string name() { return name_; }
  inline addr featurize(const char *key, addr length) {
    return featurize_(key, length);
  }
  inline std::string translate(addr feature) { return translate_(feature); };
  inline addr featurize(const std::string &key) {
    return featurize(key.c_str(), key.length());
  }

  virtual ~Featurizer(){};
  Featurizer(const Featurizer &) = delete;
  Featurizer &operator=(const Featurizer &) = delete;
  Featurizer(Featurizer &&) = delete;
  Featurizer &operator=(Featurizer &&) = delete;

protected:
  Featurizer(){};

private:
  virtual std::string recipe_() = 0;
  virtual addr featurize_(const char *key, addr length) = 0;
  virtual std::string translate_(addr feature) = 0;
  std::string name_ = "";
};
} // namespace cottontail
#endif // COTTONTAIL_SRC_FEATURIZER_H_
