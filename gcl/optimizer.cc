#include "gcl/optimizer.h"

#include <memory>

#include "gcl/parse.h"

namespace cottontail {
namespace gcl {

std::shared_ptr<SExpression>
Optimizer::optimize(std::shared_ptr<SExpression> expr, Warren *warren) {
  (void)warren;
  return expr;
}

} // namespace gcl
} // namespace cottontail
