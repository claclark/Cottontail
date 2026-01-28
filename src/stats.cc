#include "src/stats.h"

#include <memory>
#include <string>

#include "meadowlark/meadowlark.h"
#include "meadowlark/tf-idf_stats.h"
#include "src/core.h"
#include "src/df_stats.h"
#include "src/field_stats.h"
#include "src/hopper.h"
#include "src/idf_stats.h"
#include "src/warren.h"

namespace cottontail {
std::shared_ptr<Stats> Stats::make(std::shared_ptr<Warren> warren,
                                   std::string *error) {
  if (meadowlark::is_meadow(warren)) {
    return meadowlark::TfIdfStats::make("", warren, error);
  }
  std::string container_key = "container";
  std::string container_value;
  if (!warren->get_parameter(container_key, &container_value, error))
    return nullptr;
  std::string stats_key = "statistics";
  std::string stats_name;
  std::string stats_recipe = "";
  if (!warren->get_parameter(stats_key, &stats_name, error))
    return nullptr;
  if (stats_name == "" && container_key != "")
    stats_name = "idf";
  std::shared_ptr<Stats> stats =
      Stats::make(stats_name, stats_recipe, warren, error);
  return stats;
}

std::shared_ptr<Stats> Stats::make(const std::string &name,
                                   const std::string &recipe,
                                   std::shared_ptr<Warren> warren,
                                   std::string *error) {
  if (meadowlark::is_meadow(warren)) {
    if (name == "tf-idf") {
      return meadowlark::TfIdfStats::make(recipe, warren, error);
    } else {
      safe_error(error) = "No meadowlark stats called: " + name;
      return nullptr;
    }
  }
  std::shared_ptr<Stats> stats;
  if (name == "") {
    stats = std::shared_ptr<Stats>(new Stats(warren));
  } else if (name == "idf") {
    stats = IdfStats::make(recipe, warren, error);
    if (stats == nullptr)
      return nullptr;
    stats->name_ = name;
  } else if (name == "df" || name == "tf") {
    stats = DfStats::make(recipe, warren, error);
    if (stats == nullptr)
      return nullptr;
    stats->name_ = name;
  } else if (name == "field") {
    stats = FieldStats::make(recipe, warren, error);
    if (stats == nullptr)
      return nullptr;
    stats->name_ = name;
  } else {
    safe_error(error) = "No Stats named: " + name;
    stats = nullptr;
  }
  return stats;
}

bool Stats::check(const std::string &name, const std::string &recipe,
                  std::string *error) {
  if (name == "") {
    return true;
  } else if (name == "tf-idf") {
    return meadowlark::TfIdfStats::check(recipe, error);
  } else if (name == "idf") {
    return IdfStats::check(recipe, error);
  } else if (name == "df" || name == "tf") {
    return DfStats::check(recipe, error);
  } else if (name == "field") {
    return FieldStats::check(recipe, error);
  } else {
    safe_error(error) = "No Stats named: " + name;
    return false;
  }
}

std::unique_ptr<Hopper> Stats::container_hopper_() {
  std::string container_query = "";
  if (!warren_->get_parameter("container", &container_query))
    return std::make_unique<EmptyHopper>();
  if (container_query == "")
    container_query = warren_->default_container();
  if (container_query == "")
    return std::make_unique<EmptyHopper>();
  std::unique_ptr<cottontail::Hopper> hopper =
      warren_->hopper_from_gcl(container_query);
  if (hopper == nullptr)
    return std::make_unique<EmptyHopper>();
  return hopper;
}

std::unique_ptr<Hopper> content_hopper(std::shared_ptr<Warren> warren,
                                       std::string *error) {
  std::string content_query = "";
  if (!warren->get_parameter("content", &content_query, error))
    return nullptr;
  if (content_query == "" &&
      !warren->get_parameter("container", &content_query, error))
    return nullptr;
  if (content_query == "")
    content_query = warren->default_container();
  if (content_query == "") {
    safe_error(error) = "No items to rank defined by warren";
    return nullptr;
  }
  std::unique_ptr<cottontail::Hopper> hopper =
      warren->hopper_from_gcl(content_query, error);
  if (hopper == nullptr) {
    safe_error(error) = "No items to rank defined by warren";
    return nullptr;
  }
  return hopper;
}

} // namespace cottontail
