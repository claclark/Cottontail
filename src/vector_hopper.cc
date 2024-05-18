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
  void uat(addr k) { hopper_->uat(k, &p_, &q_, &v_); }

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

class UatElement : public Element {
public:
  UatElement() = delete;
  UatElement(Hopper *hopper, size_t index, addr k) : Element(hopper, index) {
    uat(k);
  };
};

class InnerTauCompare {
public:
  bool operator()(const Element &a, const Element &b) {
    return !(
        (a.q() < b.q()) ||
        ((a.q() == b.q()) &&
         ((a.p() > b.p()) || ((a.p() == b.p()) && (a.index() > b.index())))));
  };
};

class InnerUatCompare {
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
  std::priority_queue<TauElement, std::vector<TauElement>, InnerTauCompare>
      tau_queue_;
  std::priority_queue<UatElement, std::vector<UatElement>, InnerUatCompare>
      uat_queue_;
};

void InnerHopper::tau_(addr k, addr *p, addr *q, fval *v) {
  if (k == maxfinity) {
    *p = *q = maxfinity;
    *v = 0.0;
    return;
  }
  if (k < tau_k_) {
    std::priority_queue<TauElement, std::vector<TauElement>, InnerTauCompare>
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

void InnerHopper::uat_(addr k, addr *p, addr *q, fval *v) {
  if (k == minfinity) {
    *p = *q = minfinity;
    *v = 0.0;
    return;
  }
  if (k > uat_k_) {
    std::priority_queue<UatElement, std::vector<UatElement>, InnerUatCompare>
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

VectorHopper::VectorHopper(std::vector<std::unique_ptr<Hopper>> *hoppers) {
  for (auto &hopper : *hoppers)
    hoppers_.push_back(std::move(hopper));
}

std::unique_ptr<Hopper>
VectorHopper::make(std::vector<std::unique_ptr<Hopper>> *hoppers, bool outer,
                   std::string *error) {
  assert(!outer);
  if (hoppers == nullptr)
    return std::make_unique<EmptyHopper>();
  std::unique_ptr<Hopper> hopper;
  if (hoppers->size() == 0)
    hopper = std::make_unique<EmptyHopper>();
  else if (hoppers->size() == 1)
    hopper = std::move((*hoppers)[0]);
  else
    hopper =  std::make_unique<InnerHopper>(hoppers);
  hoppers->clear();
  return hopper;
}

addr VectorHopper::L_(addr k) {
  addr l_max = minfinity;
  for (auto &hopper : hoppers_) {
    addr l = hopper->L(k);
    l_max = std::max(l_max, l);
  }
  return l_max;
}

addr VectorHopper::R_(addr k) {
  addr r_min = maxfinity;
  for (auto &hopper : hoppers_) {
    addr r = hopper->R(k);
    r_min = std::min(r_min, r);
  }
  return r_min;
}
} // namespace gcl
} // namespace cottontail
