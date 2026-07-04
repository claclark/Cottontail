#ifndef COTTONTAIL_GCL_MATERIALIZE_H_
#define COTTONTAIL_GCL_MATERIALIZE_H_

#ifndef COTTONTAIL_GCL_MATERIALIZE_LAZY
#define COTTONTAIL_GCL_MATERIALIZE_LAZY 0
#endif

#if COTTONTAIL_GCL_MATERIALIZE_LAZY
#include <map>
#endif
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
  void tau_(addr k, addr *p, addr *q, fval *v) final;
  void rho_(addr k, addr *p, addr *q, fval *v) final;
  void uat_(addr k, addr *p, addr *q, fval *v) final;
  void ohr_(addr k, addr *p, addr *q, fval *v) final;

#if COTTONTAIL_GCL_MATERIALIZE_LAZY
  struct CacheEntry {
    addr k;
    addr p;
    addr q;
    fval v;
  };

  std::map<addr, CacheEntry> tau_cache_;
  std::map<addr, CacheEntry> rho_cache_;
  std::map<addr, CacheEntry> uat_cache_;
  std::map<addr, CacheEntry> ohr_cache_;
#else
  void materialize();
  bool materialized_ = false;
#endif
};

} // namespace gcl
} // namespace cottontail

#endif // COTTONTAIL_GCL_MATERIALIZE_H_
