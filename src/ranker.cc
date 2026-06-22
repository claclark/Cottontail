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
#include "gcl/gcl.h"
#include "src/hopper.h"
#include "src/parameters.h"
#include "gcl/parse.h"
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

bool trec(std::shared_ptr<Warren> warren, const std::string &stats_name,
          const std::string &stats_recipe, const std::string &pipeline,
          std::map<std::string, std::string> queries,
          std::map<std::string, std::vector<std::string>> *results,
          std::string *error, size_t threads, addr *time) {
  if (time != nullptr)
    *time = 0;
  if (queries.size() == 0)
    return true;
  assert(results != nullptr);
  results->clear();
  std::vector<std::pair<std::string, std::string>> work;
  for (auto &q : queries)
    work.push_back(q);
  threads = allowed_threads(threads);
  threads = std::min(threads, work.size());
  bool end_warren = false;
  if (!warren->started()) {
    warren->start();
    end_warren = true;
  }
  std::mutex clone_lock;
  std::mutex state_lock;
  std::vector<addr> worker_times(threads, 0);
  bool stop = false;

  auto stopping = [&]() {
    std::lock_guard<std::mutex> _(state_lock);
    return stop;
  };

  auto fail = [&](const std::string &local_error) {
    std::lock_guard<std::mutex> _(state_lock);
    if (!stop) {
      safe_set(error) = local_error;
      stop = true;
    }
  };

  auto solver = [&](size_t i) {
    std::string local_error;
    std::shared_ptr<Warren> local_warren;
    {
      std::lock_guard<std::mutex> _(clone_lock);
      if (stopping())
        return;
      local_warren = warren->clone(&local_error);
    }
    if (local_warren == nullptr) {
      fail(local_error);
      return;
    }
    std::shared_ptr<Stats> local_stats;
    local_stats =
        Stats::make(stats_name, stats_recipe, local_warren, &local_error);
    if (local_stats == nullptr) {
      local_warren->end();
      fail(local_error);
      return;
    }
    std::shared_ptr<cottontail::Ranker> rank =
        cottontail::Ranker::from_pipeline(pipeline, local_stats, &local_error);
    if (rank == nullptr) {
      local_warren->end();
      fail(local_error);
      return;
    }
    std::unique_ptr<cottontail::Hopper> id_hopper = local_stats->id_hopper();
    if (id_hopper == nullptr) {
      local_warren->end();
      fail("Can't find identifiers");
      return;
    }
    addr start = now();
    for (size_t j = i; j < work.size(); j += threads) {
      if (stopping())
        break;
      const std::string &topic = work[j].first;
      const std::string &query = work[j].second;
      std::vector<cottontail::RankingResult> ranking = (*rank)(query);
      std::vector<std::string> topic_results;
      topic_results.reserve(ranking.size());
      for (size_t r = 0; r < ranking.size(); r++) {
        addr p, q;
        id_hopper->tau(ranking[r].container_p(), &p, &q);
        if (q <= ranking[r].container_q())
          topic_results.push_back(local_stats->translate(p, q));
      }
      {
        std::lock_guard<std::mutex> _(state_lock);
        if (stop)
          break;
        (*results)[topic] = std::move(topic_results);
      }
    }
    worker_times[i] = now() - start;
    local_warren->end();
  };
  std::vector<std::thread> workers;
  for (size_t i = 0; i < threads; i++)
    workers.emplace_back(std::thread(solver, i));
  for (auto &worker : workers)
    worker.join();
  if (end_warren)
    warren->end();
  if (!stop && time != nullptr)
    *time = *std::max_element(worker_times.begin(), worker_times.end());
  return !stop;
}

} // namespace cottontail
