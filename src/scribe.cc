#include "src/scribe.h"

#include <memory>
#include <string>

#include "src/annotator.h"
#include "src/appender.h"
#include "src/committable.h"
#include "src/core.h"

namespace cottontail {

std::shared_ptr<Scribe> Scribe::null(std::string *error) {
  std::shared_ptr<Scribe> scribe = std::shared_ptr<Scribe>(new Scribe());
  scribe->annotator_ = Annotator::make("null", "", error);
  if (scribe->annotator_ == nullptr)
    return nullptr;
  scribe->appender_ = Appender::make("null", "", error);
  if (scribe->appender_ == nullptr)
    return nullptr;
  return scribe;
}

bool Scribe::set_(const std::string &key, const std::string &value,
                  std::string *error) {
  return true;
}

} // namespace cottontail
