#ifndef COTTONTAIL_SRC_FLUFFLE_H_
#define COTTONTAIL_SRC_FLUFFLE_H_

#include <memory>
#include <mutex>
#include <vector>

#include "src/warren.h"

namespace cottontail {

struct Fluffle {
  Fluffle() = default;
  static std::shared_ptr<Fluffle> make() {
    return std::make_shared<Fluffle>();
  };
  std::mutex lock;
  std::vector<std::shared_ptr<Warren>> warrens;
};

} // namespace cottontail

#endif // COTTONTAIL_SRC_FLUFFLE_H_
