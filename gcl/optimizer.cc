#include "gcl/optimizer.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "gcl/parse.h"
#include "src/warren.h"

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

addr Optimizer::estimate_memory(const std::string &query, Warren *warren) {
  std::string error;
  std::shared_ptr<SExpression> expr = SExpression::from_string(query, &error);
  return estimate_memory(expr, warren);
}

addr Optimizer::estimate_memory(std::shared_ptr<SExpression> expr,
                                Warren *warren) {
  if (expr == nullptr || warren == nullptr)
    return 0;
  expr = expr->expand_phrases(warren->tokenizer());
  if (expr->is_error())
    return 0;
  std::set<addr> features;
  std::vector<std::shared_ptr<SExpression>> pending = {expr};
  while (!pending.empty()) {
    std::shared_ptr<SExpression> current = pending.back();
    pending.pop_back();
    if (current == nullptr)
      continue;
    if (current->kind_ == TERM) {
      features.insert(warren->featurizer()->featurize(current->term_));
    } else {
      pending.insert(pending.end(), current->subx_.begin(),
                     current->subx_.end());
    }
  }
  addr postings = 0;
  for (addr feature : features)
    postings += warren->idx()->count(feature);
  return postings * 3 * sizeof(addr);
}

std::shared_ptr<SExpression>
Optimizer::optimize(std::shared_ptr<SExpression> expr, Warren *warren) {
  if (expr == nullptr || expr->is_error())
    return expr;
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
  if (expr->kind_ == ERROR)
    return expr;
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
