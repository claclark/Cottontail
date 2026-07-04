#ifndef COTTONTAIL_GCL_OPTIMIZER_H_
#define COTTONTAIL_GCL_OPTIMIZER_H_

#include <memory>
#include <string>
#include <vector>

#include "gcl/parse.h"

namespace cottontail {

class Warren;

namespace gcl {

class Optimizer {
public:
  static std::shared_ptr<SExpression>
  optimize(std::shared_ptr<SExpression> expr, Warren *warren);
  static void enable();
  static void disable();

private:
  Optimizer() = delete;

  // Helpers that need direct access to SExpression internals.
  static std::shared_ptr<SExpression>
  optimize_(std::shared_ptr<SExpression> expr, bool materialize_inside,
            bool *materialize_me);
  static std::shared_ptr<SExpression>
  materialize(std::shared_ptr<SExpression> expr);
};

} // namespace gcl
} // namespace cottontail

#endif // COTTONTAIL_GCL_OPTIMIZER_H_
