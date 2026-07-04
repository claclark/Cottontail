#include "gcl/materialize.h"

#if COTTONTAIL_GCL_MATERIALIZE_LAZY

namespace cottontail {
namespace gcl {

void Materialize::tau_(addr k, addr *p, addr *q, fval *v) {
  auto entry = tau_cache_.lower_bound(k);
  if (entry != tau_cache_.end() && entry->second.k <= k) {
    *p = entry->second.p;
    *q = entry->second.q;
    *v = entry->second.v;
    return;
  }
  expr_->tau(k, p, q, v);
  auto existing = tau_cache_.find(*p);
  if (existing != tau_cache_.end() && existing->second.k < k)
    k = existing->second.k;
  tau_cache_[*p] = {k, *p, *q, *v};
}

void Materialize::rho_(addr k, addr *p, addr *q, fval *v) {
  auto entry = rho_cache_.lower_bound(k);
  if (entry != rho_cache_.end() && entry->second.k <= k) {
    *p = entry->second.p;
    *q = entry->second.q;
    *v = entry->second.v;
    return;
  }
  expr_->rho(k, p, q, v);
  auto existing = rho_cache_.find(*q);
  if (existing != rho_cache_.end() && existing->second.k < k)
    k = existing->second.k;
  rho_cache_[*q] = {k, *p, *q, *v};
}

void Materialize::uat_(addr k, addr *p, addr *q, fval *v) {
  auto entry = uat_cache_.upper_bound(k);
  if (entry != uat_cache_.begin()) {
    --entry;
    if (k <= entry->second.k) {
      *p = entry->second.p;
      *q = entry->second.q;
      *v = entry->second.v;
      return;
    }
  }
  expr_->uat(k, p, q, v);
  auto existing = uat_cache_.find(*q);
  if (existing != uat_cache_.end() && existing->second.k > k)
    k = existing->second.k;
  uat_cache_[*q] = {k, *p, *q, *v};
}

void Materialize::ohr_(addr k, addr *p, addr *q, fval *v) {
  auto entry = ohr_cache_.upper_bound(k);
  if (entry != ohr_cache_.begin()) {
    --entry;
    if (k <= entry->second.k) {
      *p = entry->second.p;
      *q = entry->second.q;
      *v = entry->second.v;
      return;
    }
  }
  expr_->ohr(k, p, q, v);
  auto existing = ohr_cache_.find(*p);
  if (existing != ohr_cache_.end() && existing->second.k > k)
    k = existing->second.k;
  ohr_cache_[*p] = {k, *p, *q, *v};
}

} // namespace gcl
} // namespace cottontail

#else

#include <memory>

#include "src/array_hopper.h"
#include "src/core.h"
#include "src/hopper.h"
#include "src/simple_posting.h"

namespace cottontail {
namespace gcl {
namespace {

std::shared_ptr<SimplePosting> make_posting() {
  return SimplePostingFactory::make(nullptr, nullptr)
      ->posting_from_feature(null_feature);
}

} // namespace

void Materialize::materialize() {
  if (materialized_)
    return;
  addr p, q;
  fval v;
  expr_->tau(minfinity + 1, &p, &q, &v);
  if (p == maxfinity) {
    expr_ = std::make_unique<EmptyHopper>();
    materialized_ = true;
    return;
  }
  addr first_p = p, first_q = q;
  fval first_v = v;
  expr_->tau(p + 1, &p, &q, &v);
  if (p == maxfinity) {
    expr_ = std::make_unique<SingletonHopper>(first_p, first_q, first_v);
    materialized_ = true;
    return;
  }
  std::shared_ptr<SimplePosting> posting = make_posting();
  posting->push(first_p, first_q, first_v);
  do {
    posting->push(p, q, v);
    expr_->tau(p + 1, &p, &q, &v);
  } while (p < maxfinity);
  expr_ = ArrayHopper::make(posting);
  materialized_ = true;
}

void Materialize::tau_(addr k, addr *p, addr *q, fval *v) {
  materialize();
  expr_->tau(k, p, q, v);
}

void Materialize::rho_(addr k, addr *p, addr *q, fval *v) {
  materialize();
  expr_->rho(k, p, q, v);
}

void Materialize::uat_(addr k, addr *p, addr *q, fval *v) {
  materialize();
  expr_->uat(k, p, q, v);
}

void Materialize::ohr_(addr k, addr *p, addr *q, fval *v) {
  materialize();
  expr_->ohr(k, p, q, v);
}

} // namespace gcl
} // namespace cottontail

#endif // COTTONTAIL_GCL_MATERIALIZE_LAZY
