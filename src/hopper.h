#ifndef COTTONTAIL_SRC_HOPPER_H_
#define COTTONTAIL_SRC_HOPPER_H_

#include "src/core.h"

namespace cottontail {

class Hopper {
public:
  inline addr L(addr k) { return L_(k); }
  inline addr R(addr k) { return R_(k); }

  inline void tau(addr k, addr *p, addr *q, fval *v) {
    if (k != tau_k_) {
      tau_k_ = k;
      tau_v_ = 0.0;
      tau_(tau_k_, &tau_p_, &tau_q_, &tau_v_);
    }
    *p = tau_p_;
    *q = tau_q_;
    *v = tau_v_;
    return;
  };
  inline void rho(addr k, addr *p, addr *q, fval *v) {
    if (k != rho_k_) {
      rho_k_ = k;
      rho_v_ = 0.0;
      rho_(rho_k_, &rho_p_, &rho_q_, &rho_v_);
    }
    *p = rho_p_;
    *q = rho_q_;
    *v = rho_v_;
    return;
  };
  inline void uat(addr k, addr *p, addr *q, fval *v) {
    if (k != uat_k_) {
      uat_k_ = k;
      uat_v_ = 0.0;
      uat_(uat_k_, &uat_p_, &uat_q_, &uat_v_);
    }
    *p = uat_p_;
    *q = uat_q_;
    *v = uat_v_;
    return;
  };
  inline void ohr(addr k, addr *p, addr *q, fval *v) {
    if (k != ohr_k_) {
      ohr_k_ = k;
      ohr_v_ = 0.0;
      ohr_(ohr_k_, &ohr_p_, &ohr_q_, &ohr_v_);
    }
    *p = ohr_p_;
    *q = ohr_q_;
    *v = ohr_v_;
    return;
  };

  inline void tau(addr k, addr *p, addr *q, addr *n) {
    tau(k, p, q, (fval *) n);
  };
  inline void rho(addr k, addr *p, addr *q, addr *n) {
    rho(k, p, q, (fval *) n);
  };
  inline void uat(addr k, addr *p, addr *q, addr *n) {
    uat(k, p, q, (fval *) n);
  };
  inline void ohr(addr k, addr *p, addr *q, addr *n) {
    ohr(k, p, q, (fval *) n);
  };

  inline void tau(addr k, addr *p, addr *q) {
    fval v;
    tau(k, p, q, &v);
  };
  inline void rho(addr k, addr *p, addr *q) {
    fval v;
    rho(k, p, q, &v);
  };
  inline void uat(addr k, addr *p, addr *q) {
    fval v;
    uat(k, p, q, &v);
  };
  inline void ohr(addr k, addr *p, addr *q) {
    fval v;
    ohr(k, p, q, &v);
  };

  virtual ~Hopper(){};

private:
  virtual addr L_(addr k);
  virtual addr R_(addr k);

  virtual void tau_(addr k, addr *p, addr *q, fval *v) = 0;
  addr tau_k_ = minfinity;
  addr tau_p_ = minfinity;
  addr tau_q_ = minfinity;
  fval tau_v_ = 0.0;

  virtual void rho_(addr k, addr *p, addr *q, fval *v) = 0;
  addr rho_k_ = minfinity;
  addr rho_p_ = minfinity;
  addr rho_q_ = minfinity;
  fval rho_v_ = 0.0;

  virtual void uat_(addr k, addr *p, addr *q, fval *v) = 0;
  addr uat_k_ = maxfinity;
  addr uat_p_ = maxfinity;
  addr uat_q_ = maxfinity;
  fval uat_v_ = 0.0;

  virtual void ohr_(addr k, addr *p, addr *q, fval *v) = 0;
  addr ohr_k_ = maxfinity;
  addr ohr_p_ = maxfinity;
  addr ohr_q_ = maxfinity;
  fval ohr_v_ = 0.0;
};

class EmptyHopper final : public Hopper {
private:
  void tau_(addr k, addr *p, addr *q, fval *v) final;
  void rho_(addr k, addr *p, addr *q, fval *v) final;
  void uat_(addr k, addr *p, addr *q, fval *v) final;
  void ohr_(addr k, addr *p, addr *q, fval *v) final;
};

class SingletonHopper final : public Hopper {
public:
  SingletonHopper(addr p, addr q, fval v) : p_(p), q_(q), v_(v){};

private:
  void tau_(addr k, addr *p, addr *q, fval *v) final;
  void rho_(addr k, addr *p, addr *q, fval *v) final;
  void uat_(addr k, addr *p, addr *q, fval *v) final;
  void ohr_(addr k, addr *p, addr *q, fval *v) final;

  addr p_;
  addr q_;
  fval v_;
};

class FixedWidthHopper final : public Hopper {
public:
  FixedWidthHopper(addr width) : width_(width){};

private:
  void tau_(addr k, addr *p, addr *q, fval *v) final;
  void rho_(addr k, addr *p, addr *q, fval *v) final;
  void uat_(addr k, addr *p, addr *q, fval *v) final;
  void ohr_(addr k, addr *p, addr *q, fval *v) final;

  addr width_;
};
} // namespace cottontail
#endif // COTTONTAIL_SRC_HOPPER_H_
