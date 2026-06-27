#include "src/warren.h"

#include <fstream>
#include <map>
#include <sys/stat.h>

#include "src/bigwig.h"
#include "src/core.h"
#include "src/dna.h"
#include "src/hazel.h"
#include "src/owsla.h"
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
    safe_error(error) = "No Warren named: " + name;
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

namespace {
bool is_regular_file(const std::string &filename) {
  if (filename == "")
    return false;
  struct stat status;
  if (stat(filename.c_str(), &status) != 0)
    return false;
  return S_ISREG(status.st_mode);
}

std::shared_ptr<Warren> single_file_burrow(const std::string &burrow,
                                           std::string *error) {
  std::fstream in(burrow, std::ios::binary | std::ios::in);
  if (in.fail()) {
    safe_error(error) = "Can't open burrow: " + burrow;
    return nullptr;
  }
  const std::string magic = cottontail_file_magic;
  std::string actual(magic.size(), '\0');
  in.read(&actual[0], actual.size());
  if (actual != magic) {
    safe_error(error) = "Bad burrow magic number: " + burrow;
    return nullptr;
  }
  std::string dna;
  std::string line;
  bool found_blank = false;
  while (std::getline(in, line)) {
    if (line == "") {
      found_blank = true;
      break;
    }
    dna += line + "\n";
  }
  if (!found_blank) {
    safe_error(error) = "Burrow has no DNA terminator: " + burrow;
    return nullptr;
  }
  std::map<std::string, std::string> parameters;
  if (!cook(dna, &parameters, error))
    return nullptr;
  auto warren = parameters.find("warren");
  if (warren == parameters.end()) {
    safe_error(error) = "Burrow has no warren type: " + burrow;
    return nullptr;
  }
  std::string name = warren->second;
  if (name == "hazel")
    return Hazel::make(burrow, dna, error);
  safe_error(error) = "Invalid warren type: " + name;
  return nullptr;
}
}

std::shared_ptr<Warren> Warren::make(const std::string &burrow,
                                     std::string *error) {
  if (is_regular_file(burrow))
    return single_file_burrow(burrow, error);
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
  if (parameters.find("warren") != parameters.end())
    name = parameters["warren"];
  else
    name = "simple";
  return Warren::make(name, burrow, error);
}

void Warren::commit_all(std::vector<std::shared_ptr<Warren>> warrens) {
  std::vector<std::shared_ptr<Bigwig>> bigwigs;
  for (auto &warren : warrens) {
    if (warren == nullptr || warren->name() != "bigwig")
      break;
    auto bigwig = std::dynamic_pointer_cast<Bigwig>(warren);
    if (bigwig == nullptr)
      break;
    bigwigs.push_back(bigwig);
  }
  if (bigwigs.size() == warrens.size() && Bigwig::commit_all(bigwigs))
    return;
  for (auto &warren : warrens)
    warren->commit();
}

std::shared_ptr<Warren> Warren::clone_(std::string *error) {
  safe_error(error) = "Warren type does not support cloning: " + name();
  return nullptr;
}
} // namespace cottontail
