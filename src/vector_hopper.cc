#include "src/vector_hopper.h"

#include <cassert>
#include <memory>
#include <queue>
#include <vector>

#include "src/core.h"
#include "src/hopper.h"

namespace cottontail {

namespace gcl {

namespace {
class Element {
public:
  Element() = delete;
  inline size_t index() const { return index_; };
  inline addr p() const { return p_; };
  inline addr q() const { return q_; };
  inline fval v() const { return v_; };
  void tau(addr k) { hopper_->tau(k, &p_, &q_, &v_); }
  void rho(addr k) { hopper_->rho(k, &p_, &q_, &v_); }
  void uat(addr k) { hopper_->rho(k, &p_, &q_, &v_); }
  void ohr(addr k) { hopper_->rho(k, &p_, &q_, &v_); }

protected:
  Element(Hopper *hopper, size_t index) : hopper_(hopper), index_(index){};
  Hopper *hopper_;
  size_t index_;
  addr p_, q_;
  fval v_;
};

class TauElement : public Element {
public:
  TauElement() = delete;
  TauElement(Hopper *hopper, size_t index, addr k) : Element(hopper, index) {
    tau(k);
  };
};

class RhoElement : public Element {
public:
  RhoElement() = delete;
  RhoElement(Hopper *hopper, size_t index, addr k) : Element(hopper, index) {
    rho(k);
  };
};

class UatElement : public Element {
public:
  UatElement() = delete;
  UatElement(Hopper *hopper, size_t index, addr k) : Element(hopper, index) {
    uat(k);
  };
};

class OhrElement : public Element {
public:
  OhrElement() = delete;
  OhrElement(Hopper *hopper, size_t index, addr k) : Element(hopper, index) {
    uat(k);
  };
};

class InnerTauRhoCompare {
public:
  bool operator()(const Element &a, const Element &b) {
    return !(
        (a.q() < b.q()) ||
        ((a.q() == b.q()) &&
         ((a.p() > b.p()) || ((a.p() == b.p()) && (a.index() > b.index())))));
  };
};

class InnerUatOhrCompare {
public:
  bool operator()(const Element &a, const Element &b) {
    return !(
        (a.p() > b.p()) ||
        ((a.p() == b.p()) &&
         ((a.q() < b.q() || ((a.q() == b.q()) && (a.index() > b.index()))))));
  };
};
} // namespace

class InnerHopper : public VectorHopper {
public:
  InnerHopper(std::vector<std::unique_ptr<Hopper>> *hoppers)
      : VectorHopper(hoppers){};
  InnerHopper() = delete;
  virtual ~InnerHopper(){};
  InnerHopper(InnerHopper const &) = delete;
  InnerHopper &operator=(InnerHopper const &) = delete;
  InnerHopper(InnerHopper &&) = delete;
  InnerHopper &operator=(InnerHopper &&) = delete;

private:
  void tau_(addr k, addr *p, addr *q, fval *v) final;
  void rho_(addr k, addr *p, addr *q, fval *v) final;
  void uat_(addr k, addr *p, addr *q, fval *v) final;
  void ohr_(addr k, addr *p, addr *q, fval *v) final;
  std::priority_queue<TauElement, std::vector<TauElement>, InnerTauRhoCompare>
      tau_queue_;
  std::priority_queue<RhoElement, std::vector<RhoElement>, InnerTauRhoCompare>
      rho_queue_;
  std::priority_queue<UatElement, std::vector<UatElement>, InnerUatOhrCompare>
      uat_queue_;
  std::priority_queue<OhrElement, std::vector<OhrElement>, InnerUatOhrCompare>
      ohr_queue_;
};

void InnerHopper::tau_(addr k, addr *p, addr *q, fval *v) {
  if (k == maxfinity) {
    *p = *q = maxfinity;
    *v = 0.0;
    return;
  }
  if (k < tau_k_) {
    std::priority_queue<TauElement, std::vector<TauElement>, InnerTauRhoCompare>
        temp;
    for (size_t i = 0; i < hoppers_.size(); i++)
      temp.emplace(hoppers_[i].get(), i, k);
    std::swap(temp, tau_queue_);
  } else {
    while (tau_queue_.top().p() < k) {
      TauElement e = tau_queue_.top();
      tau_queue_.pop();
      e.tau(k);
      tau_queue_.push(e);
    }
  }
  tau_k_ = k;
  *p = tau_queue_.top().p();
  *q = tau_queue_.top().q();
  *v = tau_queue_.top().v();
}

void InnerHopper::rho_(addr k, addr *p, addr *q, fval *v) {
  if (k == maxfinity) {
    *p = *q = maxfinity;
    *v = 0.0;
    return;
  }
  if (k < rho_k_) {
    std::priority_queue<RhoElement, std::vector<RhoElement>, InnerTauRhoCompare>
        temp;
    for (size_t i = 0; i < hoppers_.size(); i++)
      temp.emplace(hoppers_[i].get(), i, k);
    std::swap(temp, rho_queue_);
  } else {
    addr p0 = rho_queue_.top().p() + 1;
    for (;;) {
      RhoElement e = rho_queue_.top();
      if (e.q() < k) {
        rho_queue_.pop();
        e.rho(k);
        if (e.p() < p0)
          e.tau(p0);
        rho_queue_.push(e);
      } else if (e.p() < p0) {
        rho_queue_.pop();
        e.tau(p0);
        rho_queue_.push(e);
      } else {
        break;
      }
    }
  }
  rho_k_ = k;
  *p = rho_queue_.top().p();
  *q = rho_queue_.top().q();
  *v = rho_queue_.top().v();
}

void InnerHopper::uat_(addr k, addr *p, addr *q, fval *v) {
  if (k == minfinity) {
    *p = *q = minfinity;
    *v = 0.0;
    return;
  }
  if (k > uat_k_) {
    std::priority_queue<UatElement, std::vector<UatElement>, InnerUatOhrCompare>
        temp;
    for (size_t i = 0; i < hoppers_.size(); i++)
      temp.emplace(hoppers_[i].get(), i, k);
    std::swap(temp, uat_queue_);
  } else {
    while (uat_queue_.top().q() > k) {
      UatElement e = uat_queue_.top();
      uat_queue_.pop();
      e.uat(k);
      uat_queue_.push(e);
    }
  }
  uat_k_ = k;
  *p = uat_queue_.top().p();
  *q = uat_queue_.top().q();
  *v = uat_queue_.top().v();
}

void InnerHopper::ohr_(addr k, addr *p, addr *q, fval *v) {
  if (k == minfinity) {
    *p = *q = minfinity;
    *v = 0.0;
  }
  if (k > ohr_k_) {
    std::priority_queue<OhrElement, std::vector<OhrElement>, InnerUatOhrCompare>
        temp;
    for (size_t i = 0; i < hoppers_.size(); i++)
      temp.emplace(hoppers_[i].get(), i, k);
    std::swap(temp, ohr_queue_);
  } else {
    addr q0 = ohr_queue_.top().q() - 1;
    ;
    for (;;) {
      OhrElement e = ohr_queue_.top();
      if (e.p() > k) {
        ohr_queue_.pop();
        e.ohr(k);
        if (e.q() > q0)
          e.uat(q0);
        ohr_queue_.push(e);
      } else if (e.q() > q0) {
        ohr_queue_.pop();
        e.uat(q0);
        ohr_queue_.push(e);
      } else {
        break;
      }
    }
  }
  ohr_k_ = k;
  *p = ohr_queue_.top().p();
  *q = ohr_queue_.top().q();
  *v = ohr_queue_.top().v();
}

VectorHopper::VectorHopper(std::vector<std::unique_ptr<Hopper>> *hoppers) {
  for (auto &hopper : *hoppers)
    hoppers_.push_back(std::move(hopper));
  hoppers->clear();
}

std::unique_ptr<Hopper>
VectorHopper::make(std::vector<std::unique_ptr<Hopper>> *hoppers, bool outer) {
  assert(!outer);
  std::unique_ptr<InnerHopper> hopper = std::make_unique<InnerHopper>(hoppers);
  return hopper;
}
} // namespace gcl
} // namespace cottontail
