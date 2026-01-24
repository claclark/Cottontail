#ifndef COTTONTAIL_SRC_TF_HOPPER_H_
#define COTTONTAIL_SRC_TF_HOPPER_H_

#include <memory>
#include <vector>

#include "src/core.h"
#include "src/hopper.h"

namespace cottontail {

class TfHopper final : public Hopper {
public:
  TfHopper(std::unique_ptr<Hopper> tf_hopper,
           std::unique_ptr<Hopper> content_hopper)
      : tf_hopper_(std::move(tf_hopper)),
        content_hopper_(std::move(content_hopper)){};

  virtual ~TfHopper(){};
  TfHopper(const TfHopper &) = delete;
  TfHopper &operator=(const TfHopper &) = delete;
  TfHopper(TfHopper &&) = delete;
  TfHopper &operator=(TfHopper &&) = delete;

private:
  void tau_(addr k, addr * p, addr * q, fval * v) final {
    addr p0, q0, tf;
    tf_hopper_->tau(k, &p0, &q0, &tf);
    content_hopper_->tau(p0, p, q);
    *v = (fval)tf;
  };
  void rho_(addr k, addr * p, addr * q, fval * v) final {
    addr p0, q0, tf;
    content_hopper_->rho(k, &p0, &q0);
    tf_hopper_->tau(p0, &p0, &q0, &tf);
    content_hopper_->tau(p0, p, q);
    *v = (fval)tf;
  };
  void uat_(addr k, addr * p, addr * q, fval * v) final {
    addr p0, q0, tf;
    content_hopper_->uat(k, &p0, &q0);
    tf_hopper_->ohr(p0, &p0, &q0, &tf);
    content_hopper_->ohr(p0, p, q);
    *v = (fval)tf;
  };
  void ohr_(addr k, addr * p, addr * q, fval * v) final {
    addr p0, q0, tf;
    tf_hopper_->ohr(k, &p0, &q0, &tf);
    content_hopper_->ohr(p0, p, q);
    *v = (fval)tf;
  };
  std::unique_ptr<Hopper> tf_hopper_;
  std::unique_ptr<Hopper> content_hopper_;
};

} // namespace cottontail
#endif // COTTONTAIL_SRC_TF_HOPPER_H_

