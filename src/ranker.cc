#include "src/ranker.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <exception>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "src/core.h"
#include "src/gcl.h"
#include "src/hopper.h"
#include "src/parameters.h"
#include "src/parse.h"
#include "src/ranking.h"
#include "src/stats.h"
#include "src/stopwords.h"

namespace cottontail {

namespace {
class RankingContext {
public:
  RankingContext(std::shared_ptr<Stats> stats) : stats_(stats){};
  RankingContext() = delete;
  RankingContext(const RankingContext &) = delete;
  RankingContext &operator=(const RankingContext &) = delete;
  RankingContext(RankingContext &&) = delete;
  RankingContext &operator=(RankingContext &&) = delete;

  void clear(const std::string &query) {
    raw_query_ = query;
    weighted_query_.clear();
    parameters_.clear();
    ranking_.clear();
  };

  std::map<std::string, fval> parameters() { return parameters_; };
  std::vector<RankingResult> ranking() { return ranking_; };

private:
  void cook() {
    if (weighted_query_.size() == 0 && raw_query_ != "") {
      std::vector<std::string> terms = stats_->tokenizer()->split(raw_query_);
      for (auto &term : terms)
        if (weighted_query_.find(term) == weighted_query_.end())
          weighted_query_[term] = 1.0;
        else
          weighted_query_[term] += 1.0;
    }
  };

  // friend class all RankingContextTransformer;
  friend class BM25Transformer;
  friend class ExpansionTransformer;
  friend class LMDTransformer;
  friend class ParameterTransformer;
  friend class RandomParameterTransformer;
  friend class StemTransformer;
  friend class StopTransformer;

  std::shared_ptr<Stats> stats_;
  std::string raw_query_;
  std::map<std::string, fval> weighted_query_;
  std::map<std::string, fval> parameters_;
  std::vector<RankingResult> ranking_;
};

class RankingContextTransformer {
public:
  void transform(class RankingContext *context) { return transform_(context); };

  virtual ~RankingContextTransformer(){};
  RankingContextTransformer(const RankingContextTransformer &) = default;
  RankingContextTransformer &
  operator=(const RankingContextTransformer &) = default;

  static std::shared_ptr<RankingContextTransformer> from_name(std::string name);

protected:
  RankingContextTransformer() = default;

private:
  virtual void transform_(class RankingContext *context) = 0;
};

class BM25Transformer : public RankingContextTransformer {
public:
  virtual ~BM25Transformer(){};

private:
  void transform_(class RankingContext *context) {
    if (context->weighted_query_.size() > 0)
      context->ranking_ = bm25_ranking(
          context->stats_, context->weighted_query_, context->parameters_);
    else
      context->ranking_ = bm25_ranking(context->stats_, context->raw_query_,
                                       context->parameters_);
  }
};

class LMDTransformer : public RankingContextTransformer {
public:
  virtual ~LMDTransformer(){};

private:
  void transform_(class RankingContext *context) {
    if (context->weighted_query_.size() > 0)
      context->ranking_ =
          lmd_ranking(context->stats_->warren(), context->weighted_query_,
                      context->parameters_);
    else
      context->ranking_ = lmd_ranking(
          context->stats_->warren(), context->raw_query_, context->parameters_);
  }
};

class ExpansionTransformer : public RankingContextTransformer {
public:
  virtual ~ExpansionTransformer(){};

protected:
  void expand(std::shared_ptr<Stats> stats,
              const std::vector<RankingResult> &ranking,
              const std::map<std::string, fval> &parameters,
              std::map<std::string, fval> *weighted_query) {
    expand_(stats, ranking, parameters, weighted_query);
  };

private:
  void transform_(class RankingContext *context) {
    if (context->ranking_.size() == 0)
      return;
    context->cook();
    expand(context->stats_, context->ranking_, context->parameters_,
           &context->weighted_query_);
  };
  virtual void expand_(std::shared_ptr<Stats> stats,
                       const std::vector<RankingResult> &ranking,
                       const std::map<std::string, fval> &parameters,
                       std::map<std::string, fval> *weighted_query) = 0;
};

class RSJTransformer : public ExpansionTransformer {
public:
  virtual ~RSJTransformer(){};

private:
  void expand_(std::shared_ptr<Stats> stats,
               const std::vector<RankingResult> &ranking,
               const std::map<std::string, fval> &parameters,
               std::map<std::string, fval> *weighted_query) {
    rsj_prf(stats->warren(), ranking, parameters, weighted_query);
  };
};

class KLDTransformer : public ExpansionTransformer {
public:
  virtual ~KLDTransformer(){};

private:
  void expand_(std::shared_ptr<Stats> stats,
               const std::vector<RankingResult> &ranking,
               const std::map<std::string, fval> &parameters,
               std::map<std::string, fval> *weighted_query) {
    kld_prf(stats->warren(), ranking, parameters, weighted_query);
  };
};

class StopTransformer : public RankingContextTransformer {
public:
  virtual ~StopTransformer(){};
  StopTransformer(const std::vector<std::string> &list) {
    for (auto &&word : list)
      stopwords_.insert(word);
  };

private:
  StopTransformer(){};
  std::set<std::string> stopwords_;
  void transform_(class RankingContext *context) {
    context->cook();
    std::map<std::string, fval> stopped_query;
    for (auto &term : context->weighted_query_)
      if (stopwords_.find(term.first) == stopwords_.end())
        stopped_query[term.first] = term.second;
    if (stopped_query.size() > 0)
      context->weighted_query_ = stopped_query;
  }
};

class StemTransformer : public RankingContextTransformer {
public:
  virtual ~StemTransformer(){};

private:
  void transform_(class RankingContext *context) {
    context->cook();
    std::map<std::string, fval> stemmed_query;
    for (auto &term : context->weighted_query_) {
      std::string stemmed_term = context->stats_->stemmer()->stem(term.first);
      if (stemmed_query.find(stemmed_term) == stemmed_query.end())
        stemmed_query[stemmed_term] = term.second;
      else
        stemmed_query[stemmed_term] += term.second;
    }
    context->weighted_query_ = stemmed_query;
  };
};

class ParameterTransformer : public RankingContextTransformer {
public:
  ParameterTransformer(const std::string &key, fval value)
      : key_(key), value_(value){};
  virtual ~ParameterTransformer(){};

private:
  std::string key_;
  fval value_;
  void transform_(class RankingContext *context) {
    context->parameters_[key_] = value_;
  };
};

class RandomParameterTransformer : public RankingContextTransformer {
public:
  RandomParameterTransformer(const std::string &ranker_name) {
    std::shared_ptr<Parameters> generator =
        Parameters::from_ranker_name(ranker_name);
    okay_ = (generator != nullptr);
    if (okay_)
      random_parameters_ = generator->random();
  }
  virtual ~RandomParameterTransformer(){};

  bool okay() { return okay_; };

private:
  bool okay_;
  std::map<std::string, fval> random_parameters_;
  void transform_(class RankingContext *context) {
    if (okay_) {
      for (auto &parameter : random_parameters_)
        context->parameters_[parameter.first] = parameter.second;
    }
  }
};

std::shared_ptr<RankingContextTransformer>
RankingContextTransformer::from_name(std::string transformation) {
  if (transformation.find("=") != std::string::npos) {
    std::string key = transformation.substr(0, transformation.find("="));
    std::string value_string =
        transformation.substr(transformation.find("=") + 1);
    if (key == "" || value_string == "")
      return nullptr;
    fval value;
    try {
      value = std::stof(value_string);
    } catch (std::exception &e) {
      return nullptr;
    }
    return std::make_shared<ParameterTransformer>(key, value);
  }
  if (transformation.find(":") != std::string::npos) {
    std::string name = transformation.substr(0, transformation.find(":"));
    std::string argument = transformation.substr(transformation.find(":") + 1);
    if (name == "random") {
      std::shared_ptr<RandomParameterTransformer> transformer =
          std::make_shared<RandomParameterTransformer>(argument);
      if (transformer->okay())
        return transformer;
      else
        return nullptr;
    } else {
      return nullptr;
    }
  }
  if (transformation == "bm25")
    return std::make_shared<BM25Transformer>();
  else if (transformation == "kld")
    return std::make_shared<KLDTransformer>();
  else if (transformation == "lmd")
    return std::make_shared<LMDTransformer>();
  else if (transformation == "stem")
    return std::make_shared<StemTransformer>();
  else if (transformation == "stop")
    return std::make_shared<StopTransformer>(cast2019_stopwords);
  else if (transformation == "rsj")
    return std::make_shared<RSJTransformer>();
  else
    return nullptr;
}

class RankingPipeline : public Ranker {
public:
  RankingPipeline(std::shared_ptr<Stats> stats) : stats_(stats){};
  virtual ~RankingPipeline(){};
  RankingPipeline(const RankingPipeline &) = delete;
  RankingPipeline &operator=(const RankingPipeline &) = delete;
  RankingPipeline(RankingPipeline &&) = delete;
  RankingPipeline &operator=(RankingPipeline &&) = delete;

  void add_transformer(std::shared_ptr<RankingContextTransformer> transformer) {
    transformers_.push_back(transformer);
  };

private:
  std::shared_ptr<Stats> stats_;
  std::vector<std::shared_ptr<RankingContextTransformer>> transformers_;
  virtual std::vector<RankingResult>
  rank_(const std::string &query,
        std::map<std::string, fval> *parameters) final {
    RankingContext context(stats_);
    context.clear(query);
    for (auto &transformer : transformers_)
      transformer->transform(&context);
    if (parameters != nullptr)
      *parameters = context.parameters();
    return context.ranking();
  }
};

} // namespace

std::shared_ptr<Ranker> Ranker::from_pipeline(const std::string &pipeline,
                                              std::shared_ptr<Stats> stats,
                                              std::string *error) {
  std::shared_ptr<RankingPipeline> rpp =
      std::make_shared<RankingPipeline>(stats);
  std::regex ws_re("\\s+");
  std::vector<std::string> stages{
      std::sregex_token_iterator(pipeline.begin(), pipeline.end(), ws_re, -1),
      {}};
  for (auto &stage : stages) {
    std::shared_ptr<RankingContextTransformer> transformer =
        RankingContextTransformer::from_name(stage);
    if (transformer != nullptr) {
      rpp->add_transformer(transformer);
    } else {
      safe_error(error) = "Invalid ranking stage: " + stage;
      return nullptr;
    }
  }
  return rpp;
}

std::shared_ptr<Ranker> Ranker::from_pipeline(const std::string &pipeline,
                                              std::shared_ptr<Warren> warren,
                                              std::string *error) {
  std::shared_ptr<Stats> stats = Stats::make(warren, error);
  if (stats == nullptr)
    return nullptr;
  return Ranker::from_pipeline(pipeline, stats, error);
}

bool trec(std::shared_ptr<Stats> stats, const std::string &pipeline,
          std::map<std::string, std::string> queries,
          std::map<std::string, std::vector<std::string>> *results,
          std::string *error, size_t threads) {
  if (queries.size() == 0)
    return true;
  assert(results != nullptr);
  {
    std::shared_ptr<cottontail::Ranker> rank =
        cottontail::Ranker::from_pipeline(pipeline, stats, error);
    if (rank == nullptr)
      return false;
    addr p, q;
    std::unique_ptr<cottontail::Hopper> id_hopper = stats->id_hopper();
    if (id_hopper == nullptr) {
      safe_error(error) = "Can't find identifiers";
      return false;
    }
    id_hopper->tau(minfinity + 1, &p, &q);
    if (p == maxfinity) {
      safe_error(error) = "Can't find identifiers";
      return false;
    }
  }
  std::vector<std::string> topics;
  for (auto &q : queries)
    topics.push_back(q.first);
  if (threads == 0)
    threads = std::thread::hardware_concurrency() + 1;
  threads = std::min(threads, topics.size());
  std::mutex sync;
  bool stop = false;
  auto solver = [&](size_t i) {
    std::string local_error;
    std::shared_ptr<Stats> local_stats;
    {
      std::lock_guard<std::mutex> _(sync);
      if (stop)
        return;
      local_stats = stats->clone(&local_error);
      if (local_stats == nullptr) {
        safe_set(error) = local_error;
        stop = true;
        return;
      }
    }
    std::shared_ptr<cottontail::Ranker> rank =
        cottontail::Ranker::from_pipeline(pipeline, stats, &local_error);
    if (rank == nullptr) {
      std::lock_guard<std::mutex> _(sync);
      if (stop)
        return;
      safe_set(error) = local_error;
      stop = true;
      return;
    }
    std::unique_ptr<cottontail::Hopper> id_hopper = local_stats->id_hopper();
    if (id_hopper == nullptr) {
      std::lock_guard<std::mutex> _(sync);
      if (stop)
        return;
      safe_set(error) = "Can't find identifiers";
      stop = true;
      return;
    }
    for (size_t j = i; j < topics.size(); j += threads) {
      std::string topic, query;
      {
        std::lock_guard<std::mutex> _(sync);
        if (stop)
          return;
        topic = topics[j];
        query = queries[topic];
      }
      std::vector<cottontail::RankingResult> ranking = (*rank)(query);
      if (ranking.size() == 0) {
        std::lock_guard<std::mutex> _(sync);
        if (stop)
          return;
        (*results)[topic].clear();
      } else {
        for (size_t r = 0; r < ranking.size(); r++) {
          addr p, q;
          id_hopper->tau(ranking[r].container_q(), &p, &q);
          if (q <= ranking[i].container_q()) {
            std::string docno = local_stats->warren()->txt()->translate(p, q);
            {
              std::lock_guard<std::mutex> _(sync);
              if (stop)
                return;
              (*results)[topic].push_back(docno);
            }
          }
        }
      }
    }
  };
  std::vector<std::thread> workers;
  for (size_t i = 0; i < threads; i++)
    workers.emplace_back(std::thread(solver, i));
  for (auto &worker : workers)
    worker.join();
  return true;
}

} // namespace cottontail
