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
#include "src/stemmer.h"
#include "src/tokenizer.h"
#include "src/warren.h"

namespace cottontail {
std::shared_ptr<Stats> Stats::make(std::shared_ptr<Warren> warren,
                                   std::string *error) {
  if (meadowlark::is_meadow(warren)) {
    return Stats::make("tf-idf", "", warren, error);
  }
  std::string stats_key = "statistics";
  std::string stats_name;
  std::string stats_recipe = "";
  if (!warren->get_parameter(stats_key, &stats_name, error))
    return nullptr;
  if (stats_name == "") {
    std::string container_key = "container";
    std::string container_value;
    if (!warren->get_parameter(container_key, &container_value, error))
      return nullptr;
    if (container_value != "") {
      stats_name = "idf";
    } else {
      safe_error(error) = "No ranking statistic in warren";
      return nullptr;
    }
  }
  std::shared_ptr<Stats> stats =
      Stats::make(stats_name, stats_recipe, warren, error);
  return stats;
}

std::shared_ptr<Stats> Stats::make(const std::string &name,
                                   const std::string &recipe,
                                   std::shared_ptr<Warren> warren,
                                   std::string *error) {
  std::shared_ptr<Stats> stats;
  if (meadowlark::is_meadow(warren)) {
    if (name == "tf-idf") {
      stats = meadowlark::TfIdfStats::make(recipe, warren, error);
    } else {
      safe_error(error) = "No meadowlark stats called: " + name;
      return nullptr;
    }
  } else if (name == "") {
    stats = std::shared_ptr<Stats>(new Stats(warren));
  } else if (name == "idf") {
    stats = IdfStats::make(recipe, warren, error);
  } else if (name == "df" || name == "tf") {
    stats = DfStats::make(recipe, warren, error);
  } else if (name == "field") {
    stats = FieldStats::make(recipe, warren, error);
  } else {
    safe_error(error) = "No Stats named: " + name;
    return nullptr;
  }
  if (stats == nullptr)
    return nullptr;
  if ((stats->stemmer_ == nullptr) &&
      ((stats->stemmer_ = warren->stemmer()) == nullptr) &&
      ((stats->stemmer_ = Stemmer::make("", "", error)) == nullptr))
    return nullptr;
  if (stats->tokenizer_ == nullptr)
    assert((stats->tokenizer_ = warren->tokenizer()) != nullptr);
  stats->name_ = name;
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

std::unique_ptr<Hopper> Stats::id_hopper_() {
  std::string id_query;
  if (!warren()->get_parameter("id", &id_query))
    return std::make_unique<EmptyHopper>();
  std::unique_ptr<cottontail::Hopper> hopper =
      warren()->hopper_from_gcl(id_query);
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
