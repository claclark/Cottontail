// This file implements query processing for the mathematical framework defined
// by the following publications.
//
// Charles L. A. Clarke.
// An Algebra for Structured Text Search.
// PhD thesis, University of Waterloo, 1996.
// https://plg.uwaterloo.ca/~claclark/phd.pdf.
//
// Charles L. A. Clarke and Gordon V. Cormack.
// Shortest-substring retrieval and ranking. ACM
// Transactions on Information Systems, 18(1):44–78, 2000.
//
// Charles L. A. Clarke, Gordon V. Cormack, and Forbes J. Burkowski.
// An algebra for structured text search and a framework for its implementation.
// The Computer Journal, 38(1):43–56, 1995.
//
// Charles L. A. Clarke, Gordon V. Cormack, and Forbes J. Burkowski.
// Schema-independent retrieval from hetrogeneous structured text.
// In 4th Annual Symposium on Document Analysis and Information Retrieval,
// pages 279–289, Las Vegas, Nevada, April 1995.

#include "src/gcl.h"

#include <algorithm>
#include <iostream>
#include <set>

#include "src/array_hopper.h"

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
  if (*p == maxfinity || *q == minfinity)
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
  if (*q == minfinity || *p == maxfinity)
    return;
  for (;;) {
    addr pp, qq;
    right_->ohr(*p, &pp, &qq);
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
  uat(qq, p, q, v);
}

void NotContaining::tau_(addr k, addr *p, addr *q, fval *v) {
  addr pp, qq;
  left_->tau(k, &pp, &qq);
  rho(qq, p, q, v);
}

void NotContaining::rho_(addr k, addr *p, addr *q, fval *v) {
  left_->rho(k, p, q, v);
  if (*q == maxfinity || *p == minfinity)
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
  if (*p == minfinity || *q == maxfinity)
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

void Merge::tau_(addr k, addr *p, addr *q, fval *v) {
  addr pl, ql, pr, qr;
  fval vl, vr;
  left_->tau(k, &pl, &ql, &vl);
  right_->tau(k, &pr, &qr, &vr);
  if (ql < qr) {
    *p = pl;
    *q = ql;
    *v = vl;
  } else if (ql > qr) {
    *p = pr;
    *q = qr;
    *v = vr;
  } else if (pl < pr) {
    *p = pr;
    *q = qr;
    *v = vr;
  } else {
    *p = pl;
    *q = ql;
    *v = vl;
  }
}

void Merge::rho_(addr k, addr *p, addr *q, fval *v) {
  if (k == minfinity) {
    *p = *q = minfinity;
    *v = 0.0;
    return;
  }
  addr l = L(k - 1);
  if (l == maxfinity) {
    *p = *q = maxfinity;
    *v = 0.0;
    return;
  }
  tau(l + 1, p, q, v);
}

void Merge::uat_(addr k, addr *p, addr *q, fval *v) {
  addr pl, ql, pr, qr;
  fval vl, vr;
  left_->uat(k, &pl, &ql, &vl);
  right_->uat(k, &pr, &qr, &vr);
  if (pl > pr) {
    *p = pl;
    *q = ql;
    *v = vl;
  } else if (pr > pl) {
    *p = pr;
    *q = qr;
    *v = vr;
  } else if (ql < qr) {
    *p = pr;
    *q = qr;
    *v = vr;
  } else {
    *p = pl;
    *q = ql;
    *v = vl;
  }
}

void Merge::ohr_(addr k, addr *p, addr *q, fval *v) {
  if (k == maxfinity) {
    *p = *q = maxfinity;
    *v = 0.0;
    return;
  }
  addr r = R(k + 1);
  if (r == minfinity) {
    *p = *q = minfinity;
    *v = 0.0;
    return;
  }
  uat(r - 1, p, q, v);
}

namespace {
std::unique_ptr<Hopper> make_link_hopper(const std::set<addr> &s) {
  if (s.size() == 0)
    return std::make_unique<EmptyHopper>();
  std::shared_ptr<addr> postings =
      cottontail::shared_array<cottontail::addr>(s.size());
  addr *p = postings.get();
  for (auto &a : s)
    *p++ = a;
  if (s.size() == 1)
    return std::make_unique<SingletonHopper>(*postings, *postings, 0.0);
  else
    return ArrayHopper::make(s.size(), postings, postings, nullptr);
}
} // namespace

void Link::tau_(addr k, addr *p, addr *q, fval *v) {
  if (!linked_) {
    addr p0, q0, v0;
    std::set<addr> s;
    for (expr_->tau(minfinity + 1, &p0, &q0, &v0); p0 < maxfinity;
         expr_->tau(p0 + 1, &p0, &q0, &v0))
      if (v0 > minfinity && v0 < maxfinity)
        s.insert(v0);
    expr_ = make_link_hopper(s);
    linked_ = true;
  }
  return expr_->tau(k, p, q, v);
}

void Link::rho_(addr k, addr *p, addr *q, fval *v) {
  if (!linked_) {
    addr p0, q0, v0;
    std::set<addr> s;
    for (expr_->rho(minfinity + 1, &p0, &q0, &v0); q0 < maxfinity;
         expr_->rho(q0 + 1, &p0, &q0, &v0))
      if (v0 > minfinity && v0 < maxfinity)
        s.insert(v0);
    expr_ = make_link_hopper(s);
    linked_ = true;
  }
  return expr_->rho(k, p, q, v);
}

void Link::uat_(addr k, addr *p, addr *q, fval *v) {
  if (!linked_) {
    addr p0, q0, v0;
    std::set<addr> s;
    for (expr_->uat(maxfinity - 1, &p0, &q0, &v0); q0 > minfinity;
         expr_->uat(q0 - 1, &p0, &q0, &v0))
      if (v0 > minfinity && v0 < maxfinity)
        s.insert(v0);
    expr_ = make_link_hopper(s);
    linked_ = true;
  }
  return expr_->uat(k, p, q, v);
}

void Link::ohr_(addr k, addr *p, addr *q, fval *v) {
  if (!linked_) {
    addr p0, q0, v0;
    std::set<addr> s;
    for (expr_->ohr(maxfinity - 1, &p0, &q0, &v0); p0 > minfinity;
         expr_->ohr(p0 - 1, &p0, &q0, &v0))
      if (v0 > minfinity && v0 < maxfinity)
        s.insert(v0);
    expr_ = make_link_hopper(s);
    linked_ = true;
  }
  return expr_->ohr(k, p, q, v);
}

} // namespace gcl
} // namespace cottontail
