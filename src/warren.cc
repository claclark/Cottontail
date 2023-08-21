#include "src/warren.h"

#include "src/core.h"
#include "src/simple_warren.h"
#include "src/stats.h"

namespace cottontail {
std::shared_ptr<Warren> Warren::make(const std::string &name,
                                     const std::string &burrow,
                                     std::string *error) {
  std::shared_ptr<Warren> warren;
  if (name == "" || name == "simple") {
    warren = SimpleWarren::make(burrow, error);
    if (warren == nullptr)
      return nullptr;
  } else {
    safe_set(error) = "No Warren named: " + name;
    return nullptr;
  }
  warren->start();
  warren->name_ = name;
  std::string container_key = "container";
  std::string container_value;
  if (!warren->get_parameter(container_key, &container_value, error))
    return nullptr;
  warren->default_container_ = container_value;
  std::string stemmer_key = "stemmer";
  std::string stemmer_value;
  if (!warren->get_parameter(stemmer_key, &stemmer_value, error))
    return nullptr;
  if (stemmer_value != "") {
    std::string empty_recipe = "";
    std::shared_ptr<Stemmer> stemmer =
        Stemmer::make(stemmer_value, empty_recipe, error);
    if (stemmer == nullptr)
      return nullptr;
    warren->stemmer_ = stemmer;
  }
  warren->end();
  return warren;
}
} // namespace cottontail
