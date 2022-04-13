#include "src/hopper.h"

#include "src/core.h"

namespace cottontail {

addr Hopper::L_(addr k) {
  addr q, p;
  uat(k, &p, &q);
  return p;
}

addr Hopper::R_(addr k) {
  addr p, q;
  tau(k, &p, &q);
  return q;
}

void EmptyHopper::tau_(addr k, addr *p, addr *q, fval *v) {
  if (k == minfinity)
    *p = *q = minfinity;
  else
    *p = *q = maxfinity;
}

void EmptyHopper::rho_(addr k, addr *p, addr *q, fval *v) {
  if (k == minfinity)
    *p = *q = minfinity;
  else
    *p = *q = maxfinity;
}

void EmptyHopper::uat_(addr k, addr *p, addr *q, fval *v) {
  if (k == maxfinity)
    *p = *q = maxfinity;
  else
    *p = *q = minfinity;
}

void EmptyHopper::ohr_(addr k, addr *p, addr *q, fval *v) {
  if (k == maxfinity)
    *p = *q = maxfinity;
  else
    *p = *q = minfinity;
}

void SingletonHopper::tau_(addr k, addr *p, addr *q, fval *v) {
  if (k == minfinity) {
    *p = *q = minfinity;
  } else if (k <= p_) {
    *p = p_;
    *q = q_;
    *v = v_;
  } else {
    *p = *q = maxfinity;
  }
}

void SingletonHopper::rho_(addr k, addr *p, addr *q, fval *v) {
  if (k == minfinity) {
    *p = *q = minfinity;
  } else if (k <= q_) {
    *p = p_;
    *q = q_;
    *v = v_;
  } else {
    *p = *q = maxfinity;
  }
}

void SingletonHopper::uat_(addr k, addr *p, addr *q, fval *v) {
  if (k == maxfinity) {
    *p = *q = maxfinity;
  } else if (k >= q_) {
    *p = p_;
    *q = q_;
    *v = v_;
  } else {
    *p = *q = minfinity;
  }
}

void SingletonHopper::ohr_(addr k, addr *p, addr *q, fval *v) {
  if (k == maxfinity) {
    *p = *q = maxfinity;
  } else if (k >= p_) {
    *p = p_;
    *q = q_;
    *v = v_;
  } else {
    *p = *q = minfinity;
  }
}

void FixedWidthHopper::tau_(addr k, addr *p, addr *q, fval *v) {
  if (k == minfinity) {
    *p = *q = minfinity;
  } else if (k <= maxfinity - width_) {
    *p = k;
    *q = k + width_ - 1;
  } else {
    *p = *q = maxfinity;
  }
}

void FixedWidthHopper::rho_(addr k, addr *p, addr *q, fval *v) {
  if (k == minfinity) {
    *p = *q = minfinity;
  } else if (k < minfinity + width_) {
    *p = minfinity + 1;
    *q = minfinity + width_;
  } else if (k < maxfinity) {
    *p = k - width_ + 1;
    *q = k;
  } else {
    *p = *q = maxfinity;
  }
}

void FixedWidthHopper::uat_(addr k, addr *p, addr *q, fval *v) {
  if (k == maxfinity) {
    *p = *q = maxfinity;
  } else if (k >= minfinity + width_) {
    *p = k - width_ + 1;
    *q = k;
  } else {
    *p = *q = minfinity;
  }
}

void FixedWidthHopper::ohr_(addr k, addr *p, addr *q, fval *v) {
  if (k == maxfinity) {
    *p = *q = maxfinity;
  } else if (k >= maxfinity - width_) {
    *p = maxfinity - width_;
    *q = maxfinity - 1;
  } else if (k > minfinity) {
    *p = k;
    *q = k + width_ - 1;
  } else {
    *p = *q = minfinity;
  }
}
}
