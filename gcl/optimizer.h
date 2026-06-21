#ifndef COTTONTAIL_GCL_OPTIMIZER_H_
#define COTTONTAIL_GCL_OPTIMIZER_H_

#include <memory>

#include "gcl/parse.h"

namespace cottontail {

class Warren;

namespace gcl {

class Optimizer {
public:
  static std::shared_ptr<SExpression>
  optimize(std::shared_ptr<SExpression> expr, Warren *warren);

private:
  Optimizer() = delete;
};

} // namespace gcl
} // namespace cottontail

#endif // COTTONTAIL_GCL_OPTIMIZER_H_
