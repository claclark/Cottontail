#include "gcl/optimizer.h"

#include <memory>
#include <string>
#include <vector>

#include "gcl/parse.h"

namespace cottontail {
namespace gcl {
namespace {
// Global switch for performance comparisons.
bool enabled = true;

bool is_materializable(Operator kind) {
  return kind == CONTAINED_IN || kind == CONTAINING ||
         kind == NOT_CONTAINED_IN || kind == NOT_CONTAINING;
}
} // namespace

std::shared_ptr<SExpression>
Optimizer::optimize(std::shared_ptr<SExpression> expr, Warren *warren) {
  if (!enabled)
    return expr;
  (void)warren;
  bool materialize_me;
  return optimize_(expr, false, &materialize_me);
}

void Optimizer::enable() { enabled = true; }

void Optimizer::disable() { enabled = false; }

std::shared_ptr<SExpression>
Optimizer::optimize_(std::shared_ptr<SExpression> expr,
                     bool materialize_inside, bool *materialize_me) {
  *materialize_me = false;
  if (expr == nullptr)
    return nullptr;
  if (expr->kind_ == MATERIALIZE)
    return expr;
  std::vector<std::shared_ptr<SExpression>> subx;
  bool materializable = is_materializable(expr->kind_);
  bool materialize_children =
      expr->kind_ == ONE_OF || (materialize_inside && !materializable);
  for (auto &sub : expr->subx_) {
    bool materialize_child;
    std::shared_ptr<SExpression> optimized =
        optimize_(sub, materialize_children, &materialize_child);
    if (materialize_children && materialize_child)
      optimized = materialize(optimized);
    subx.push_back(optimized);
  }
  std::shared_ptr<SExpression> optimized =
      SExpression::make(expr->kind_, expr->term_, expr->width_, subx);
  *materialize_me = materializable;
  return optimized;
}

std::shared_ptr<SExpression>
Optimizer::materialize(std::shared_ptr<SExpression> expr) {
  return SExpression::make(MATERIALIZE, "", 0, {expr});
}

} // namespace gcl
} // namespace cottontail
