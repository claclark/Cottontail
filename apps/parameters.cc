#include <algorithm>
#include <cassert>
#include <chrono>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <random>
#include <thread>

#include "src/cottontail.h"

constexpr size_t THREADS = 50;

void spew_metrics(const std::map<std::string, cottontail::fval> &metrics,
                  const std::string &target_metric,
                  std::ostream &where = std::cout) {
  where << "-->";
  if (metrics.find(target_metric) != metrics.end())
    where << " " << target_metric << "=" << metrics.at(target_metric);
  if (target_metric != "ap" && metrics.find("ap") != metrics.end())
    where << " ap=" << metrics.at("ap");
  if (target_metric != "p10" && metrics.find("p10") != metrics.end())
    where << " p10=" << metrics.at("p10");
  if (target_metric != "tbg" && metrics.find("tbg") != metrics.end())
    where << " tbg=" << metrics.at("tbg");
}

struct Trial {
  std::map<std::string, cottontail::fval> parameters;
  std::map<std::string, cottontail::fval> metrics;
  void spew(const std::string &target_metric, std::ostream &where = std::cout) {
    for (auto &parameter : parameters)
      where << parameter.first << "=" << parameter.second << " ";
    spew_metrics(metrics, target_metric, where);
    where << "\n";
  }
};

void magical(const std::string &target_metric,
             const std::map<std::string, std::vector<Trial>> &trials) {
  std::map<std::string, cottontail::fval> mean_metrics;
  for (auto &topic : trials) {
    assert(topic.second.size() > 0);
    Trial best = topic.second[0];
    for (size_t i = 1; i < topic.second.size(); i++)
      if (topic.second[i].metrics.at(target_metric) >
          best.metrics[target_metric])
        best = topic.second[i];
    std::cout << "Best for " << topic.first << ": ";
    best.spew(target_metric);
    cottontail::mean(best.metrics, &mean_metrics);
  }
  std::cout << "Magical parameters... \n";
  spew_metrics(mean_metrics, target_metric);
  std::cout << "\n";
}

void bootstrap(const std::string &target_metric, size_t answers,
               const std::map<std::string, std::vector<Trial>> &trials,
               const std::string program_name) {
  const size_t B = 1000; // a standard number for bootstrap iterations
  std::default_random_engine generator(
      std::chrono::system_clock::now().time_since_epoch().count());
  std::uniform_int_distribution<size_t> distribution(0, trials.size() - 1);
  std::function<size_t()> select = std::bind(distribution, generator);
  std::vector<std::string> topics;
  for (auto &topic : trials)
    topics.push_back(topic.first);
  size_t n = trials.at(topics[0]).size();
  for (auto &topic : topics)
    assert(trials.at(topic).size() == n);
  std::vector<Trial> current;
  for (size_t j = 0; j < n; j++)
    current.push_back(trials.at(topics[0])[j]);
  for (size_t i = 1; i < topics.size(); i++)
    for (size_t j = 0; j < n; j++)
      cottontail::mean(trials.at(topics[i])[j].metrics, &current[j].metrics);
  std::vector<Trial> cumulative;
  for (size_t j = 0; j < n; j++) {
    std::map<std::string, cottontail::fval> metrics; // reset averaging counter
    cottontail::mean(current[j].metrics, &metrics);
    cumulative.push_back({current[j].parameters, metrics});
  }
  for (size_t b = 0; b < B; b++) {
    std::cerr << program_name << ": bootstrap iteration " << b << "\n";
    std::vector<std::string> selected;
    for (size_t i = 0; i < topics.size(); i++) // bootstrap sample
      selected.push_back(topics[select()]);
    current.clear();
    for (size_t j = 0; j < n; j++)
      current.push_back(trials.at(selected[0])[j]);
    for (size_t i = 1; i < selected.size(); i++)
      for (size_t j = 0; j < n; j++)
        cottontail::mean(trials.at(selected[i])[j].metrics,
                         &current[j].metrics);
    for (size_t j = 0; j < n; j++) {
      cottontail::mean(current[j].metrics, &cumulative[j].metrics);
    }
  }
  std::sort(cumulative.begin(), cumulative.end(),
            [&](const auto &a, const auto &b) -> bool {
              return a.metrics.at(target_metric) > b.metrics.at(target_metric);
            });
  std::cout << "Bootstrap...\n";
  for (unsigned i = 0; i < answers && i < cumulative.size(); i++)
    cumulative[i].spew(target_metric);
  std::cout.flush();
}

void usage(std::string program_name) {
  std::cerr << "usage: " << program_name
            << " [--burrow burrow] queries qrels metric pipeline...\n";
}

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  std::string burrow = "";
  if (argc == 2 && argv[1] == std::string("--help")) {
    usage(program_name);
    return 0;
  }
  if (argc > 2 &&
      (argv[1] == std::string("-b") || argv[1] == std::string("--burrow"))) {
    burrow = argv[2];
    argc -= 2;
    argv += 2;
  }
  if (argc < 5) {
    usage(program_name);
    return 1;
  }
  size_t threads = THREADS;
  std::string queries_filename = argv[1];
  std::string qrels_filename = argv[2];
  std::string target_metric = argv[3];
  std::string pipeline;
  for (int i = 4; i < argc; i++) {
    if (i > 4)
      pipeline += " ";
    pipeline += argv[i];
  }
  std::cout << pipeline << "\n";
  std::string error;
  std::string simple = "simple";
  std::shared_ptr<cottontail::Warren> warren;
  warren = cottontail::Warren::make(simple, burrow, &error);
  if (warren == nullptr) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }
  warren->start();
  std::map<std::string, std::map<cottontail::addr, cottontail::fval>> qrels;
  if (!load_trec_qrels(warren, qrels_filename, &qrels, &error)) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }
  std::ifstream queriesf(queries_filename);
  if (queriesf.fail()) {
    std::cerr << program_name
              << ": can't open query file \"" + queries_filename + "\"\n";
    return 1;
  }
  std::string line;
  std::map<std::string, std::string> queries;
  while (std::getline(queriesf, line)) {
    std::string separator = "\t";
    if (line.find(separator) == std::string::npos)
      separator = " ";
    if (line.find(separator) == std::string::npos) {
      std::cerr << program_name << ": weird query input: " << line;
      return 1;
    }
    std::string topic = line.substr(0, line.find(separator));
    std::string query = line.substr(line.find(separator));
    if (topic == "" || query == "") {
      std::cerr << program_name << ": weird query input: " << line;
      return 1;
    }
    if (qrels.find(topic) == qrels.end()) {
      std::cerr << program_name << ": no qrels for topic \"" << topic << "\"\n";
      return 1;
    }
    if (queries.find(topic) != queries.end()) {
      std::cerr << program_name << ": multiple queries for topic \"" << topic
                << "\"\n";
      return 1;
    }
    queries[topic] = query;
  }
  if (queries.size() == 0) {
    std::cerr << program_name << ": no queries\n";
    return 1;
  }
  std::vector<Trial> runs;
  std::map<std::string, std::vector<Trial>> trials;
  std::mutex lock;
  const size_t ENOUGH = 1000;
  for (size_t i = 0; i < ENOUGH; i++) {
    std::shared_ptr<cottontail::Ranker> rank =
        cottontail::Ranker::from_pipeline(pipeline, warren, &error);
    if (rank == nullptr) {
      std::cerr << program_name << ": " << error << "\n";
      return 1;
    }
    std::map<std::string, cottontail::fval> mean_metrics;
    std::queue<std::string> workq;
    std::map<std::string, cottontail::fval> example_parameters;
    for (auto &query : queries)
      workq.push(query.first);
    auto solver = [&rank, &queries, &lock, &workq, &trials, &mean_metrics,
                   &example_parameters, &program_name, &qrels]() {
      bool done = false;
      while (!done) {
        lock.lock();
        if (workq.empty()) {
          done = true;
          lock.unlock();
        } else {
          std::string label = workq.front();
          workq.pop();
          std::string query = queries[label];
          std::map<std::string, cottontail::fval> parameters;
          lock.unlock();
          std::vector<cottontail::RankingResult> ranking =
              (*rank)(query, &parameters);
          lock.lock();
          if (ranking.size() == 0)
            std::cerr << program_name << ": no results for topic \"" << label
                      << "\"\n";
          example_parameters = parameters;
          std::map<std::string, cottontail::fval> metrics;
          cottontail::eval(qrels[label], ranking, &metrics);
          trials[label].push_back({parameters, metrics});
          cottontail::mean(metrics, &mean_metrics);
          lock.unlock();
        }
      }
    };
    lock.lock();
    std::vector<std::thread> workers;
    for (size_t j = 0; j < threads; j++)
      workers.emplace_back(std::thread(solver));
    lock.unlock();
    for (auto &worker : workers)
      worker.join();
#if 0
    for (auto &query : queries) {
      std::vector<cottontail::RankingResult> ranking =
          (*rank)(query.second, &parameters);
      if (ranking.size() == 0)
        std::cerr << program_name << ": no results for topic \"" << query.first
                  << "\"\n";
      std::map<std::string, cottontail::fval> metrics;
      cottontail::evaleqrels[query.first], ranking, &metrics);
      trials[query.first].push_back({parameters, metrics});
      cottontail::mean(metrics, &mean_metrics);
    }
#endif
    if (i == 0 && mean_metrics.find(target_metric) == mean_metrics.end()) {
      std::cerr << program_name << ": " << target_metric
                << " is not a metric we compute\n";
      return 1;
    }
    runs.push_back({example_parameters, mean_metrics});
    std::cerr << program_name << ": " << i << ": ";
    runs.back().spew(target_metric, std::cerr);
    std::cerr.flush();
  }
  const size_t ANSWERS = 20;
  std::sort(runs.begin(), runs.end(),
            [&](const auto &a, const auto &b) -> bool {
              return a.metrics.at(target_metric) > b.metrics.at(target_metric);
            });
  std::cout << "Best parameters...\n";
  for (size_t i = 0; i < ANSWERS && i < runs.size(); i++)
    runs[i].spew(target_metric);
  std::cout << "Worst parameters...\n";
  for (size_t i = 0; i < ANSWERS && i < runs.size(); i++)
    runs[runs.size() - i - 1].spew(target_metric);
  std::cout.flush();
  magical(target_metric, trials);
  std::cout.flush();
  bootstrap(target_metric, ANSWERS, trials, program_name);
  std::cout.flush();
  warren->end();
  return 0;
}
