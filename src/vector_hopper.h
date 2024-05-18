#ifndef COTTONTAIL_SRC_VECTOR_HOPPER_H_
#define COTTONTAIL_SRC_VECTOR_HOPPER_H_

#include <memory>
#include <vector>

#include "src/core.h"
#include "src/hopper.h"

namespace cottontail {

namespace gcl {

class VectorHopper : public Hopper {
public:
  static std::unique_ptr<Hopper>
  make(std::vector<std::unique_ptr<Hopper>> *hoppers, bool outer,
       std::string *error = nullptr);
  VectorHopper() = delete;
  virtual ~VectorHopper(){};
  VectorHopper(VectorHopper const &) = delete;
  VectorHopper &operator=(VectorHopper const &) = delete;
  VectorHopper(VectorHopper &&) = delete;
  VectorHopper &operator=(VectorHopper &&) = delete;

protected:
  VectorHopper(std::vector<std::unique_ptr<Hopper>> *hoppers);
  addr L_(addr k) final;
  addr R_(addr k) final;
  std::vector<std::unique_ptr<Hopper>> hoppers_;
  addr tau_k_ = maxfinity;
  addr uat_k_ = minfinity;
};

} // namespace gcl
} // namespace cottontail

#endif // COTTONTAIL_SRC_VECTOR_HOPPER_H_
