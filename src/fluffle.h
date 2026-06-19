#ifndef COTTONTAIL_SRC_FLUFFLE_H_
#define COTTONTAIL_SRC_FLUFFLE_H_

#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

#include "src/owsla.h"
#include "src/warren.h"

namespace cottontail {

struct Fluffle {
  Fluffle() = default;
  static std::shared_ptr<Fluffle> make() {
    std::shared_ptr<Fluffle> fluffle = std::make_shared<Fluffle>();
    fluffle->parameters =
        std::make_shared<std::map<std::string, std::string>>();
    fluffle->cache = std::make_shared<OwslaCache>();
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
  std::set<std::shared_ptr<Owsla>> merging;
  std::vector<std::shared_ptr<Owsla>> warrens;
  std::vector<HazelMergeRecovery> hazel_merges;
  std::shared_ptr<std::map<std::string, std::string>> parameters;
  std::shared_ptr<OwslaCache> cache;
  std::shared_ptr<Working> working;
};

} // namespace cottontail

#endif // COTTONTAIL_SRC_FLUFFLE_H_
