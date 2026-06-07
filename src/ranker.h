#ifndef COTTONTAIL_SRC_RANKER_H_
#define COTTONTAIL_SRC_RANKER_H_

#include <map>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "src/core.h"
#include "src/ranking.h"
#include "src/stats.h"
#include "src/warren.h"

namespace cottontail {

class Ranker {
public:
  std::vector<RankingResult>
  operator()(const std::string &query,
             std::map<std::string, fval> *parameters = nullptr) {
    return rank_(query, parameters);
  };

  virtual ~Ranker(){};
  Ranker(const Ranker &) = delete;
  Ranker &operator=(const Ranker &) = delete;
  Ranker(Ranker &&) = delete;
  Ranker &operator=(Ranker &&) = delete;

  static std::shared_ptr<Ranker> from_pipeline(const std::string &pipeline,
                                               std::shared_ptr<Stats> stats,
                                               std::string *error = nullptr);
  static std::shared_ptr<Ranker> from_pipeline(const std::string &pipeline,
                                               std::shared_ptr<Warren> warren,
                                               std::string *error = nullptr);

protected:
  Ranker() = default;

private:
  virtual std::vector<RankingResult>
  rank_(const std::string &query, std::map<std::string, fval> *parameters) = 0;
};

bool trec(std::shared_ptr<Warren> warren, const std::string &stats_name,
          const std::string &stats_recipe, const std::string &pipeline,
          std::map<std::string, std::string> queries,
          std::map<std::string, std::vector<std::string>> *results,
          std::string *error, size_t threads, addr *time = nullptr);

} // namespace cottontail

#endif // COTTONTAIL_SRC_BUILDER_H_
