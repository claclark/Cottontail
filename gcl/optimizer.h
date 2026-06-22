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

  static std::shared_ptr<SExpression> optimize_(std::shared_ptr<SExpression> expr,
                                                Warren *warren);
  static std::shared_ptr<SExpression>
  rewrite_contained_in_all_of(std::shared_ptr<SExpression> expr,
                              Warren *warren);
  static bool collect_all_of_atoms(std::shared_ptr<SExpression> expr,
                                   std::vector<std::shared_ptr<SExpression>>
                                       *atoms);
  static bool is_atomic(std::shared_ptr<SExpression> expr);
  static addr cost(std::shared_ptr<SExpression> expr, Warren *warren);
  static std::shared_ptr<SExpression>
  materialize(std::shared_ptr<SExpression> expr);
  static std::shared_ptr<SExpression>
  make(Operator kind, const std::string &term, addr width,
       const std::vector<std::shared_ptr<SExpression>> &subx);
  static std::shared_ptr<SExpression>
  clone(std::shared_ptr<SExpression> expr);
};

} // namespace gcl
} // namespace cottontail

#endif // COTTONTAIL_GCL_OPTIMIZER_H_
