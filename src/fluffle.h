#ifndef COTTONTAIL_SRC_FLUFFLE_H_
#define COTTONTAIL_SRC_FLUFFLE_H_

#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include "src/warren.h"

namespace cottontail {

struct Fluffle {
  Fluffle() = default;
  static std::shared_ptr<Fluffle> make() {
    std::shared_ptr<Fluffle> fluffle = std::make_shared<Fluffle>();
    fluffle->parameters =
        std::make_shared<std::map<std::string, std::string>>();
    return fluffle;
  };
  std::mutex lock;
  addr address = 0;
  addr sequence = 0;
  std::vector<std::shared_ptr<Warren>> warrens;
  std::shared_ptr<std::map<std::string, std::string>> parameters;
};

} // namespace cottontail

#endif // COTTONTAIL_SRC_FLUFFLE_H_
