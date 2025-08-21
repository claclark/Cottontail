#include "src/json_txt.h"

#include <map>
#include <memory>
#include <string>

#include "src/core.h"
#include "src/json.h"
#include "src/recipe.h"
#include "src/scribe.h"
#include "src/txt.h"

namespace cottontail {
namespace {
bool should_wrap(const std::map<std::string, std::string> &parameters) {
  auto it = parameters.find("json");
  if (it != parameters.end())
    return okay(it->second);
  else
    return false;
}

bool should_wrap(const std::string &recipe) {
  if (recipe == "json")
    return true;
  std::map<std::string, std::string> parameters;
  if (cook(recipe, &parameters))
    return should_wrap(parameters);
  else
    return false;
}
} // namespace

std::shared_ptr<Txt> JsonTxt::wrap(const std::string &recipe,
                                   std::shared_ptr<Txt> txt,
                                   std::string *error) {
  if (should_wrap(recipe))
    return wrap(txt, error);
  else
    return txt;
}

std::shared_ptr<Txt>
JsonTxt::wrap(const std::map<std::string, std::string> &parameters,
              std::shared_ptr<Txt> txt, std::string *error) {
  if (should_wrap(parameters))
    return wrap(txt, error);
  else
    return txt;
}

std::shared_ptr<Txt> JsonTxt::wrap(std::shared_ptr<Txt> txt,
                                   std::string *error) {
  if (txt == nullptr)
    return nullptr;
  std::shared_ptr<JsonTxt> jtxt = std::shared_ptr<JsonTxt>(new JsonTxt());
  if (jtxt == nullptr) {
    safe_error(error) = "Failed to create JsonTxt wrapper";
    return nullptr;
  }
  jtxt->txt_ = txt;
  return jtxt;
}

bool JsonTxt::transaction_(std::string *error) {
  return txt_->transaction(error);
}

bool JsonTxt::ready_() { return txt_->ready(); }

void JsonTxt::commit_() { txt_->commit(); }

void JsonTxt::abort_() { txt_->abort(); }

std::string JsonTxt::name_() { return txt_->name(); }

std::string JsonTxt::recipe_() {
  std::string recipe = txt_->recipe();
  std::map<std::string, std::string> parameters;
  if (recipe != "" && !cook(recipe, &parameters))
    return recipe;
  parameters["jason"] = okay(true);
  return freeze(parameters);
}

std::shared_ptr<Txt> JsonTxt::clone_(std::string *error) {
  return wrap(txt_->clone(error));
}

std::string JsonTxt::translate_(addr p, addr q) {
  return json_translate(txt_->translate(p, q));
}

std::string JsonTxt::raw_(addr p, addr q) {
  return txt_->translate(p, q);
}

addr JsonTxt::tokens_() { return txt_->tokens(); }

bool JsonTxt::range_(addr *p, addr *q) { return txt_->range(p, q); }

} // namespace cottontail
