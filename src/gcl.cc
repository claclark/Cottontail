#include "src/gcl.h"

#include <algorithm>
#include <iostream>

namespace cottontail {

namespace gcl {

void Combinational::tau_(addr k, addr *p, addr *q, fval *v) {
  *p = L(*q = R(k));
}

void Combinational::rho_(addr k, addr *p, addr *q, fval *v) {
  addr ll;
  if (k == minfinity)
    *p = *q = minfinity;
  else if ((ll = L(k - 1)) == maxfinity)
    *p = *q = maxfinity;
  else
    tau(ll + 1, p, q, v);
}

void Combinational::uat_(addr k, addr *p, addr *q, fval *v) {
  *q = R(*p = L(k));
}

void Combinational::ohr_(addr k, addr *p, addr *q, fval *v) {
  addr rr;
  if (k == maxfinity)
    *p = *q = maxfinity;
  else if ((rr = R(k + 1)) == minfinity)
    *p = *q = minfinity;
  else
    uat(rr - 1, p, q, v);
}

addr And::L_(addr k) { return std::min(left_->L(k), right_->L(k)); }

addr And::R_(addr k) { return std::max(left_->R(k), right_->R(k)); }

addr Or::L_(addr k) { return std::max(left_->L(k), right_->L(k)); }

addr Or::R_(addr k) { return std::min(left_->R(k), right_->R(k)); }

addr FollowedBy::L_(addr k) {
  switch (addr ll = right_->L(k)) {
  case minfinity:
    return minfinity;
  case maxfinity:
    return maxfinity;
  default:
    return left_->L(ll - 1);
  }
}

addr FollowedBy::R_(addr k) {
  switch (addr rr = left_->R(k)) {
  case minfinity:
    return minfinity;
  case maxfinity:
    return maxfinity;
  default:
    return right_->R(rr + 1);
  }
}

void ContainedIn::tau_(addr k, addr *p, addr *q, fval *v) {
  for (;;) {
    addr pp, qq;
    left_->tau(k, p, q, v);
    if (*p == maxfinity)
      return;
    right_->rho(*q, &pp, &qq);
    if (pp <= *p)
      return;
    k = pp;
  }
}

void ContainedIn::rho_(addr k, addr *p, addr *q, fval *v) {
  addr pp, qq;
  left_->rho(k, &pp, &qq);
  tau(pp, p, q, v);
}

void ContainedIn::uat_(addr k, addr *p, addr *q, fval *v) {
  for (;;) {
    addr pp, qq;
    left_->uat(k, p, q, v);
    if (*q == minfinity)
      return;
    right_->ohr(*p, &pp, &qq);
    if (qq >= *q)
      return;
    k = qq;
  }
}

void ContainedIn::ohr_(addr k, addr *p, addr *q, fval *v) {
  addr pp, qq;
  left_->ohr(k, &pp, &qq);
  uat(qq, p, q, v);
}

void Containing::tau_(addr k, addr *p, addr *q, fval *v) {
  addr pp, qq;
  left_->tau(k, &pp, &qq);
  rho(qq, p, q, v);
}

void Containing::rho_(addr k, addr *p, addr *q, fval *v) {
  for (;;) {
    addr pp, qq;
    left_->rho(k, p, q, v);
    if (*q == maxfinity)
      return;
    right_->tau(*p, &pp, &qq);
    if (qq <= *q)
      return;
    k = qq;
  }
}

void Containing::uat_(addr k, addr *p, addr *q, fval *v) {
  addr pp, qq;
  left_->uat(k, &pp, &qq);
  ohr(pp, p, q, v);
}

void Containing::ohr_(addr k, addr *p, addr *q, fval *v) {
  for (;;) {
    addr pp, qq;
    left_->ohr(k, p, q, v);
    if (*p == minfinity)
      return;
    right_->uat(*q, &pp, &qq);
    if (pp >= *p)
      return;
    k = pp;
  }
}

void NotContainedIn::tau_(addr k, addr *p, addr *q, fval *v) {
  left_->tau(k, p, q, v);
  if (*p == maxfinity)
    return;
  for (;;) {
    addr pp, qq;
    right_->rho(*q, &pp, &qq);
    if (pp > *p)
      return;
    left_->rho(qq + 1, p, q, v);
    if (*p == maxfinity)
      return;
  }
}

void NotContainedIn::rho_(addr k, addr *p, addr *q, fval *v) {
  addr pp, qq;
  left_->rho(k, &pp, &qq);
  tau(pp, p, q, v);
}

void NotContainedIn::uat_(addr k, addr *p, addr *q, fval *v) {
  left_->uat(k, p, q, v);
  if (*q == minfinity)
    return;
  for (;;) {
    addr pp, qq;
    right_->ohr(*q, &pp, &qq);
    if (qq < *q)
      return;
    left_->ohr(pp - 1, p, q, v);
    if (*q == minfinity)
      return;
  }
}

void NotContainedIn::ohr_(addr k, addr *p, addr *q, fval *v) {
  addr pp, qq;
  left_->ohr(k, &pp, &qq);
  uat(pp, p, q, v);
}

void NotContaining::tau_(addr k, addr *p, addr *q, fval *v) {
  addr pp, qq;
  left_->tau(k, &pp, &qq);
  rho(qq, p, q, v);
}

void NotContaining::rho_(addr k, addr *p, addr *q, fval *v) {
  left_->rho(k, p, q, v);
  if (*q == maxfinity)
    return;
  for (;;) {
    addr pp, qq;
    right_->tau(*p, &pp, &qq);
    if (qq > *q)
      return;
    left_->tau(pp + 1, p, q, v);
    if (*q == maxfinity)
      return;
  }
}

void NotContaining::uat_(addr k, addr *p, addr *q, fval *v) {
  addr pp, qq;
  left_->uat(k, &pp, &qq);
  ohr(pp, p, q, v);
}

void NotContaining::ohr_(addr k, addr *p, addr *q, fval *v) {
  left_->ohr(k, p, q, v);
  if (*p == minfinity)
    return;
  for (;;) {
    addr pp, qq;
    right_->uat(*q, &pp, &qq);
    if (pp < *p)
      return;
    left_->uat(qq - 1, p, q, v);
    if (*p == minfinity)
      return;
  }
}
} // namespace gcl
} // namespace cottontail
