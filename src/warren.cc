#include "src/warren.h"

#include "src/bigwig.h"
#include "src/core.h"
#include "src/dna.h"
#include "src/recipe.h"
#include "src/simple_warren.h"
#include "src/stats.h"
#include "src/working.h"

namespace cottontail {
std::shared_ptr<Warren> Warren::make(const std::string &name,
                                     const std::string &burrow,
                                     std::string *error) {
  std::shared_ptr<Warren> warren;
  std::string the_burrow;
  if (burrow == "")
    the_burrow = DEFAULT_BURROW;
  else
    the_burrow = burrow;
  if (name == "" || name == "simple") {
    warren = SimpleWarren::make(the_burrow, error);
    if (warren == nullptr)
      return nullptr;
  } else if (name == "bigwig") {
    warren = Bigwig::make(the_burrow, error);
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

std::shared_ptr<Warren> Warren::make(const std::string &burrow,
                                     std::string *error) {
  std::string the_burrow;
  if (burrow == "")
    the_burrow = DEFAULT_BURROW;
  else
    the_burrow = burrow;
  std::shared_ptr<Working> working = Working::make(the_burrow, error);
  if (working == nullptr)
    return nullptr;
  std::string dna;
  if (!read_dna(working, &dna, error))
    return nullptr;
  std::map<std::string, std::string> parameters;
  if (!cook(dna, &parameters, error))
    return nullptr;
  std::string name;
  if (parameters.find("warren") != parameters.end()) {
    name = parameters["warren"];
  } else {
    safe_set(error) = "No warren name in burrow dna: " + the_burrow;
    return nullptr;
  }
  return Warren::make(name, burrow, error);
}
} // namespace cottontail
