#include "src/array_hopper.h"

#define NDEBUG 1
#include <cassert>
#include <memory>

#include "src/core.h"
#include "src/hopper.h"

namespace cottontail {

namespace {

inline addr hopping(const addr *addrs, addr n, addr *current, addr k) {
  if (addrs[*current] == k)
    return addrs[*current];

  addr low, high, hop;
  if (addrs[*current] < k) {
    if (addrs[n - 1] < k)
      return maxfinity;
    low = *current;
    assert(addrs[low] < k && addrs[n - 1] >= k);
    for (hop = 1; low + hop < n && addrs[low + hop] < k; hop *= 2)
      ;
    if (low + hop >= n)
      high = n - 1;
    else
      high = low + hop;
  } else {
    assert(addrs[*current] > k);
    if (addrs[0] >= k) {
      if (k == minfinity) {
        return minfinity;
      } else {
        *current = 0;
        return addrs[0];
      }
    }
    high = *current;
    assert(addrs[0] < k && addrs[high] > k);
    for (hop = 1; high - hop >= 0 && addrs[high - hop] >= k; hop *= 2)
      ;
    if (high - hop < 0)
      low = 0;
    else
      low = high - hop;
  }

  assert(addrs[low] < k && addrs[high] >= k);
  while (high - low > 1) {
    addr probe = low + (high - low) / 2;
    if (addrs[probe] < k)
      low = probe;
    else
      high = probe;
  }
  *current = high;
  return addrs[high];
}

inline addr gnippoh(const addr *addrs, addr n, addr *current, addr k) {
  if (addrs[*current] == k)
    return addrs[*current];

  addr low, high, hop;
  if (addrs[*current] < k) {
    if (addrs[n - 1] <= k) {
      if (k == maxfinity) {
        return maxfinity;
      } else {
        *current = n - 1;
        return addrs[n - 1];
      }
    }
    low = *current;
    assert(addrs[low] < k && addrs[n - 1] > k);
    for (hop = 1; low + hop < n && addrs[low + hop] <= k; hop *= 2)
      ;
    if (low + hop >= n)
      high = n - 1;
    else
      high = low + hop;
  } else {
    assert(addrs[*current] > k);
    if (addrs[0] > k)
      return minfinity;
    high = *current;
    assert(addrs[0] <= k && addrs[high] > k);
    for (hop = 1; high - hop >= 0 && addrs[high - hop] > k; hop *= 2)
      ;
    if (high - hop < 0)
      low = 0;
    else
      low = high - hop;
  }

  assert(addrs[low] <= k && addrs[high] > k);
  while (high - low > 1) {
    addr probe = low + (high - low) / 2;
    if (addrs[probe] <= k)
      low = probe;
    else
      high = probe;
  }
  *current = low;
  return addrs[low];
}
} // namespace

addr ArrayHopper::L_(addr k) {
  wait();
  switch (gnippoh(qostings_, n_, &current_, k)) {
  case maxfinity:
    return maxfinity;
  case minfinity:
    return minfinity;
  default:
    return postings_[current_];
  }
}

addr ArrayHopper::R_(addr k) {
  wait();
  switch (hopping(postings_, n_, &current_, k)) {
  case maxfinity:
    return maxfinity;
  case minfinity:
    return minfinity;
  default:
    return qostings_[current_];
  }
}

void ArrayHopper::tau_(addr k, addr *p, addr *q, fval *v) {
  wait();
  switch (*p = hopping(postings_, n_, &current_, k)) {
  case maxfinity:
  case minfinity:
    *q = *p;
    break;
  default:
    *q = qostings_[current_];
    if (fostings_ != nullptr)
      *v = fostings_[current_];
    break;
  }
}

void ArrayHopper::rho_(addr k, addr *p, addr *q, fval *v) {
  wait();
  switch (*q = hopping(qostings_, n_, &current_, k)) {
  case maxfinity:
  case minfinity:
    *p = *q;
    break;
  default:
    *p = postings_[current_];
    if (fostings_ != nullptr)
      *v = fostings_[current_];
    break;
  }
}

void ArrayHopper::uat_(addr k, addr *p, addr *q, fval *v) {
  wait();
  switch (*q = gnippoh(qostings_, n_, &current_, k)) {
  case maxfinity:
  case minfinity:
    *p = *q;
    break;
  default:
    *p = postings_[current_];
    if (fostings_ != nullptr)
      *v = fostings_[current_];
    break;
  }
}

void ArrayHopper::ohr_(addr k, addr *p, addr *q, fval *v) {
  wait();
  switch (*p = gnippoh(postings_, n_, &current_, k)) {
  case maxfinity:
  case minfinity:
    *q = *p;
    break;
  default:
    *q = qostings_[current_];
    if (fostings_ != nullptr)
      *v = fostings_[current_];
    break;
  }
}

} // namespace cottontail
