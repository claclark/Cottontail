#include "src/stats.h"

#include <memory>
#include <string>

#include "src/core.h"
#include "src/df_stats.h"
#include "src/hopper.h"
#include "src/idf_stats.h"
#include "src/warren.h"

namespace cottontail {
std::shared_ptr<Stats> Stats::make(const std::string &name,
                                   const std::string &recipe,
                                   std::shared_ptr<Warren> warren,
                                   std::string *error) {
  std::shared_ptr<Stats> stats;
  if (name == "") {
    stats = std::shared_ptr<Stats>(new Stats(warren));
  } else if (name == "idf") {
    stats = IdfStats::make(recipe, warren, error);
    if (stats == nullptr)
      return nullptr;
    stats->name_ = name;
  } else if (name == "df") {
    stats = DfStats::make(recipe, warren, error);
    if (stats == nullptr)
      return nullptr;
    stats->name_ = name;
  } else {
    safe_set(error) = "No Stats named: " + name;
    stats = nullptr;
  }
  return stats;
}

bool Stats::check(const std::string &name, const std::string &recipe,
                  std::string *error) {
  if (name == "") {
    return true;
  } else if (name == "idf") {
    return IdfStats::check(recipe, error);
  } else if (name == "df") {
    return DfStats::check(recipe, error);
  } else {
    safe_set(error) = "No Stats named: " + name;
    return false;
  }
}
} // namespace cottontail
