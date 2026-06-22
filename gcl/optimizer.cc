#include "gcl/optimizer.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "gcl/parse.h"
#include "src/warren.h"

namespace cottontail {
namespace gcl {
namespace {
// Global switch for performance comparisons. Optimization is opt-in for now.
bool enabled = false;
} // namespace

std::shared_ptr<SExpression>
Optimizer::optimize(std::shared_ptr<SExpression> expr, Warren *warren) {
  if (!enabled)
    return expr;
  return optimize_(expr, warren);
}

void Optimizer::enable() { enabled = true; }

void Optimizer::disable() { enabled = false; }

std::shared_ptr<SExpression>
Optimizer::optimize_(std::shared_ptr<SExpression> expr, Warren *warren) {
  if (expr == nullptr)
    return nullptr;
  std::vector<std::shared_ptr<SExpression>> subx;
  for (auto &sub : expr->subx_)
    subx.push_back(optimize_(sub, warren));
  std::shared_ptr<SExpression> optimized =
      make(expr->kind_, expr->term_, expr->width_, subx);
  return rewrite_contained_in_all_of(optimized, warren);
}

std::shared_ptr<SExpression>
Optimizer::rewrite_contained_in_all_of(std::shared_ptr<SExpression> expr,
                                       Warren *warren) {
  if (expr == nullptr || warren == nullptr || expr->kind_ != CONTAINED_IN ||
      expr->subx_.size() != 2)
    return expr;
  if (!is_atomic(expr->subx_[1]))
    return expr;
  std::vector<std::shared_ptr<SExpression>> atoms;
  if (!collect_all_of_atoms(expr->subx_[0], &atoms) || atoms.size() < 3)
    return expr;
  struct Atom {
    std::shared_ptr<SExpression> expr;
    addr cost;
    size_t index;
  };
  std::vector<Atom> sorted;
  for (size_t i = 0; i < atoms.size(); i++)
    sorted.push_back({atoms[i], cost(atoms[i], warren), i});
  std::stable_sort(sorted.begin(), sorted.end(),
                   [](const Atom &a, const Atom &b) {
                     if (a.cost != b.cost)
                       return a.cost < b.cost;
                     return a.index < b.index;
                   });
  std::shared_ptr<SExpression> container = expr->subx_[1];
  std::shared_ptr<SExpression> current =
      materialize(make(CONTAINED_IN, "", 0,
                       {make(ALL_OF, "", 0,
                             {clone(sorted[0].expr), clone(sorted[1].expr)}),
                        clone(container)}));
  for (size_t i = 2; i < sorted.size(); i++)
    current = materialize(make(CONTAINED_IN, "", 0,
                               {make(ALL_OF, "", 0,
                                     {current, clone(sorted[i].expr)}),
                                clone(container)}));
  return current;
}

bool Optimizer::collect_all_of_atoms(
    std::shared_ptr<SExpression> expr,
    std::vector<std::shared_ptr<SExpression>> *atoms) {
  if (expr == nullptr || atoms == nullptr)
    return false;
  if (expr->kind_ == ALL_OF) {
    if (expr->subx_.size() == 0)
      return false;
    for (auto &sub : expr->subx_)
      if (!collect_all_of_atoms(sub, atoms))
        return false;
    return true;
  }
  if (!is_atomic(expr))
    return false;
  atoms->push_back(expr);
  return true;
}

bool Optimizer::is_atomic(std::shared_ptr<SExpression> expr) {
  return expr != nullptr && (expr->kind_ == TERM || expr->kind_ == FIXED);
}

addr Optimizer::cost(std::shared_ptr<SExpression> expr, Warren *warren) {
  if (expr == nullptr || warren == nullptr)
    return maxfinity;
  if (expr->kind_ == TERM)
    return warren->idx()->count(warren->featurizer()->featurize(expr->term_));
  return maxfinity;
}

std::shared_ptr<SExpression>
Optimizer::materialize(std::shared_ptr<SExpression> expr) {
  return make(MATERIALIZE, "", 0, {expr});
}

std::shared_ptr<SExpression> Optimizer::make(
    Operator kind, const std::string &term, addr width,
    const std::vector<std::shared_ptr<SExpression>> &subx) {
  std::shared_ptr<SExpression> expr = std::make_shared<SExpression>();
  expr->kind_ = kind;
  expr->term_ = term;
  expr->width_ = width;
  expr->subx_ = subx;
  return expr;
}

std::shared_ptr<SExpression>
Optimizer::clone(std::shared_ptr<SExpression> expr) {
  if (expr == nullptr)
    return nullptr;
  std::vector<std::shared_ptr<SExpression>> subx;
  for (auto &sub : expr->subx_)
    subx.push_back(clone(sub));
  return make(expr->kind_, expr->term_, expr->width_, subx);
}

} // namespace gcl
} // namespace cottontail
