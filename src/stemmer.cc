#include "src/stemmer.h"

#include <memory>
#include <string>

#include "src/core.h"
#include "src/porter.h"

namespace cottontail {

std::shared_ptr<Stemmer> Stemmer::make(const std::string &name,
                                       const std::string &recipe,
                                       std::string *error) {
  std::shared_ptr<Stemmer> stemmer = nullptr;
  if (name == "" || name == "null") {
    stemmer = std::make_shared<NullStemmer>();
  } else if (name == "porter") {
    stemmer = std::make_shared<Porter>();
    if (stemmer != nullptr)
      stemmer->name_ = name;
  } else {
    safe_error(error) = "No Stemmer named: " + name;
  }
  return stemmer;
}

bool Stemmer::check(const std::string &name, const std::string &recipe,
                    std::string *error) {
  return name == "" || name == "null" || name == "porter";
}
} // namespace cottontail
