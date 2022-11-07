#include "src/idx.h"

#include <memory>
#include <string>

#include "src/core.h"
#include "src/hopper.h"
#include "src/null_idx.h"
#include "src/simple_idx.h"
#include "src/working.h"

namespace cottontail {

std::shared_ptr<Idx> Idx::make(const std::string &name,
                               const std::string &recipe, std::string *error,
                               std::shared_ptr<Working> working) {
  std::shared_ptr<Idx> idx;
  if (name == "" || name == "simple") {
    idx = SimpleIdx::make(recipe, working, error);
    if (idx != nullptr)
      idx->name_ = "simple";
  } else if (name == "null") {
    idx = NullIdx::make(recipe, error);
    if (idx != nullptr)
      idx->name_ = "null";
  } else {
    safe_set(error) = "No Idx named: " + name;
    idx = nullptr;
  }
  if (idx != nullptr)
    idx->working_ = working;
  return idx;
}

bool Idx::check(const std::string &name, const std::string &recipe,
                std::string *error) {
  if (name == "" || name == "simple") {
    return SimpleIdx::check(recipe, error);
  } else {
    safe_set(error) = "No Idx named: " + name;
    return false;
  }
}

addr Idx::count_(addr feature) {
  // Brute force implementation for convenience, but you almost certainly want
  // to replace this virtual method with one that does direct calls to your
  // index.
  std::unique_ptr<Hopper> h = hopper(feature);
  cottontail::addr k = cottontail::minfinity + 1, n = 0, p, q;
  for (h->tau(k, &p, &q); p < cottontail::maxfinity; h->tau(k, &p, &q)) {
    n++;
    k = p + 1;
  }
  return n;
}

} // namespace cottontail
