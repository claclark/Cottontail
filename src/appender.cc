#include "src/appender.h"

#include <memory>
#include <string>

#include "src/committable.h"
#include "src/core.h"
#include "src/featurizer.h"
#include "src/null_appender.h"
#include "src/simple_appender.h"
#include "src/tokenizer.h"
#include "src/working.h"

namespace cottontail {

std::shared_ptr<Appender> Appender::make(const std::string &name,
                                         const std::string &recipe,
                                         std::string *error,
                                         std::shared_ptr<Working> working,
                                         std::shared_ptr<Featurizer> featurizer,
                                         std::shared_ptr<Tokenizer> tokenizer,
                                         std::shared_ptr<Annotator> annotator) {
  std::shared_ptr<Appender> appender = nullptr;
  if (name == "" || name == "null")
    appender = NullAppender::make(recipe, error);
  else if (name == "simple")
    appender = SimpleAppender::make(recipe, error, working, featurizer,
                                    tokenizer, annotator);
  else
    safe_error(error) = "No Appender named: " + name;
  if (appender != nullptr)
    appender->name_ = name;
  return appender;
}

bool Appender::check(const std::string &name, const std::string &recipe,
                     std::string *error) {
  if (name == "" || name == "null") {
    return NullAppender::check(recipe, error);
  } else if (name == "simple") {
    return SimpleAppender::check(recipe, error);
  } else {
    safe_error(error) = "No Appender named: " + name;
    return false;
  }
}

bool Appender::recover(const std::string &name, const std::string &recipe,
                       bool commit, std::string *error,
                       std::shared_ptr<Working> working) {
  if (name == "" || name == "null") {
    return NullAppender::recover(recipe, commit, error, working);
  } else if (name == "simple") {
    return SimpleAppender::recover(recipe, commit, error, working);
  } else {
    safe_error(error) = "No Appender named: " + name;
    return false;
  }
}

} // namespace cottontail
