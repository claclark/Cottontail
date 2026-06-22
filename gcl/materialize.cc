#include "gcl/materialize.h"

#include <memory>
#include <vector>

#include "src/array_hopper.h"
#include "src/core.h"
#include "src/hopper.h"

namespace cottontail {
namespace gcl {
namespace {

std::unique_ptr<Hopper> make_hopper(const std::vector<addr> &postings,
                                    const std::vector<addr> &qostings,
                                    const std::vector<fval> &fostings) {
  if (postings.size() == 0)
    return std::make_unique<EmptyHopper>();
  if (postings.size() == 1)
    return std::make_unique<SingletonHopper>(postings[0], qostings[0],
                                             fostings[0]);
  addr n = postings.size();
  std::shared_ptr<addr> posting_storage = shared_array<addr>(n);
  std::shared_ptr<addr> qosting_storage = shared_array<addr>(n);
  std::shared_ptr<fval> fosting_storage = shared_array<fval>(n);
  for (addr i = 0; i < n; i++) {
    posting_storage.get()[i] = postings[i];
    qosting_storage.get()[i] = qostings[i];
    fosting_storage.get()[i] = fostings[i];
  }
  return ArrayHopper::make(n, posting_storage, qosting_storage,
                           fosting_storage);
}

} // namespace

void Materialize::materialize() {
  if (materialized_)
    return;
  std::vector<addr> postings;
  std::vector<addr> qostings;
  std::vector<fval> fostings;
  addr p, q;
  fval v;
  for (expr_->tau(minfinity + 1, &p, &q, &v); p < maxfinity;
       expr_->tau(p + 1, &p, &q, &v)) {
    postings.push_back(p);
    qostings.push_back(q);
    fostings.push_back(v);
  }
  expr_ = make_hopper(postings, qostings, fostings);
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
