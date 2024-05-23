#ifndef COTTONTAIL_SRC_FLUFFLE_H_
#define COTTONTAIL_SRC_FLUFFLE_H_

#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "src/warren.h"

namespace cottontail {

struct Fluffle {
  Fluffle() = default;
  static std::shared_ptr<Fluffle> make() {
    std::shared_ptr<Fluffle> fluffle = std::make_shared<Fluffle>();
    fluffle->parameters =
        std::make_shared<std::map<std::string, std::string>>();
    fluffle->max_workers =
        std::max(2 * std::thread::hardware_concurrency(), (unsigned int)2);
    return fluffle;
  };
  std::mutex lock;
  bool merge = true;
  size_t workers = 0;
  size_t max_workers;
  addr address = 0;
  addr sequence = 0;
  std::set<std::shared_ptr<Warren>> merging;
  std::vector<std::shared_ptr<Warren>> warrens;
  std::shared_ptr<std::map<std::string, std::string>> parameters;
};

} // namespace cottontail

#endif // COTTONTAIL_SRC_FLUFFLE_H_
