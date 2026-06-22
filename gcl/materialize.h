#ifndef COTTONTAIL_GCL_MATERIALIZE_H_
#define COTTONTAIL_GCL_MATERIALIZE_H_

#include <memory>

#include "gcl/gcl.h"
#include "src/core.h"
#include "src/hopper.h"

namespace cottontail {
namespace gcl {

class Materialize final : public Unary {
public:
  Materialize(std::unique_ptr<Hopper> expr) : Unary(std::move(expr)){};
  virtual ~Materialize(){};
  Materialize(Materialize const &) = delete;
  Materialize &operator=(Materialize const &) = delete;
  Materialize(Materialize &&) = delete;
  Materialize &operator=(Materialize &&) = delete;

private:
  void materialize();
  void tau_(addr k, addr *p, addr *q, fval *v) final;
  void rho_(addr k, addr *p, addr *q, fval *v) final;
  void uat_(addr k, addr *p, addr *q, fval *v) final;
  void ohr_(addr k, addr *p, addr *q, fval *v) final;

  bool materialized_ = false;
};

} // namespace gcl
} // namespace cottontail

#endif // COTTONTAIL_GCL_MATERIALIZE_H_
