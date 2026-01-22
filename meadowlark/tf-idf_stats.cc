#include "meadowlark/tf-idf_stats.h"

#include <cassert>
#include <cmath>
#include <memory>
#include <string>

#include "meadowlark/forager.h"
#include "src/core.h"
#include "src/hopper.h"
#include "src/tagging_featurizer.h"
#include "src/warren.h"

namespace cottontail {
namespace meadowlark {

std::shared_ptr<Stats> TfIdfStats::make(const std::string &recipe,
                                        std::shared_ptr<Warren> warren,
                                        std::string *error) {
  std::string label = "@" + forager_label("tf-idf", recipe);
  warren->start();
  std::shared_ptr<Hopper> hopper = warren->hopper_from_gcl(label, error);
  if (hopper == nullptr) {
    warren->end();
    return nullptr;
  }
  addr p, q;
  hopper->tau(minfinity + 1, &p, &q);
  if (p == maxfinity) {
    warren->end();
    if (recipe == "")
      safe_error(error) = "No tf-idf stats in meadow";
    else
      safe_error(error) = "No tf-idf stats in meadow for tag: " + recipe;
    return nullptr;
  }
  std::cout << warren->txt()->translate(p, q) << "\n";
  warren->end();
  return nullptr;
}

bool TfIdfStats::check(const std::string &recipe, std::string *error) {
  return false;
}

std::string TfIdfStats::recipe_() { return tag_; }

bool TfIdfStats::have_(const std::string &name) {
  return name == "content" || name == "avgl" || name == "rsj" ||
         name == "idf" || name == "tf";
}

fval TfIdfStats::avgl_() { return average_length_; }

fval TfIdfStats::idf_(const std::string &term) { return 0.0; }

fval TfIdfStats::rsj_(const std::string &term) { return 0.0; }

std::unique_ptr<Hopper> TfIdfStats::tf_hopper_(const std::string &term) {
  return nullptr;
};

virtual std::unique_ptr<Hopper> container_hopper_() { return nullptr; }

} // namespace meadowlark
} // namespace cottontail
