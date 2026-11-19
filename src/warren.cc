#include "src/warren.h"

#include <fstream>
#include <map>
#include <sys/stat.h>

#include "src/bigwig.h"
#include "src/core.h"
#include "src/hazel.h"
#include "src/recipe.h"
#include "src/simple_warren.h"
#include "src/wlock.h"

namespace cottontail {
std::shared_ptr<Warren> Warren::make(const std::string &name,
                                     const std::string &burrow,
                                     std::string *error) {
  if (name != "simple") {
    safe_error(error) =
        "The named Warren activation factory supports only Simple burrows";
    return nullptr;
  }
  return SimpleWarren::make(burrow, error);
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
} // namespace

std::shared_ptr<Warren> Warren::make(const std::string &burrow,
                                     std::string *error) {
  if (is_regular_file(burrow))
    return single_file_burrow(burrow, error);
  std::shared_ptr<WLock> wlock = WLock::obtain(burrow, error);
  if (wlock == nullptr)
    return nullptr;
  const std::string name = wlock->name();
  if (name == "simple")
    return SimpleWarren::make(wlock->working(), error);
  if (name == "bigwig")
    return Bigwig::make(wlock, error);
  safe_error(error) = name.empty() ? "Burrow has no valid DNA: " + burrow
                                   : "Invalid warren type: " + name;
  return nullptr;
}

std::shared_ptr<Warren> Warren::make(const std::string &name,
                                     const std::string &recipe,
                                     const std::string &burrow,
                                     std::string *error, bool recreate) {
  if (name != "bigwig") {
    safe_error(error) = "Cannot create Warren of type: " + name;
    return nullptr;
  }
  if (!Bigwig::check_recipe(recipe, error))
    return nullptr;
  std::shared_ptr<WLock> wlock = recreate ? WLock::recreate(name, burrow, error)
                                          : WLock::create(name, burrow, error);
  if (wlock == nullptr)
    return nullptr;
  return Bigwig::make(wlock, recipe, error);
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
