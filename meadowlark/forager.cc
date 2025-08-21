#include "meadowlark/forager.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "src/annotator.h"
#include "src/core.h"
#include "src/tokenizer.h"
#include "src/warren.h"

namespace cottontail {
namespace meadowlark {
std::shared_ptr<Forager>
Forager::make(const std::string &name, const std::string &tag,
              const std::map<std::string, std::string> &parameters,
              std::string *error)
{
  std::shared_ptr<Forager> forager = nullptr;
  safe_error(error) = "No Forager named: " + name;
  return forager;
}

}
} // namespace cottontail
