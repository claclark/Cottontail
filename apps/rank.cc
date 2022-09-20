#include <algorithm>
#include <iostream>
#include <map>
#include <mutex>
#include <regex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "src/cottontail.h"

void usage(std::string program_name) {
  std::cerr << "usage: " << program_name
            << " [--threads n] [--burrow burrow] queries pipeline...\n";
}

constexpr int THREADS = 50;

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  std::string burrow;
  if (argc == 2 && argv[1] == std::string("--help")) {
    usage(program_name);
    return 0;
  }
  size_t threads = THREADS;
  if (argc > 2 &&
      (argv[1] == std::string("-t") || argv[1] == std::string("--threads"))) {
    try {
      int t = std::stoi(argv[2]);
      if (t > 0 && t < THREADS)
        threads = t;
    } catch (std::exception &e) {
    }
    argc -= 2;
    argv += 2;
  }
  if (argc < 3) {
    usage(program_name);
    return 1;
  }
  if (argc > 2 &&
      (argv[1] == std::string("-b") || argv[1] == std::string("--burrow"))) {
    burrow = argv[2];
    argc -= 2;
    argv += 2;
  }
  if (argc < 3) {
    usage(program_name);
    return 1;
  }
  std::string queries_filename = argv[1];
  std::string pipeline;
  for (int i = 2; i < argc; i++) {
    if (i > 2)
      pipeline += " ";
    pipeline += argv[i];
  }
  std::string runid = "cottontail";
  std::string error;
  std::string simple = "simple";
  std::shared_ptr<cottontail::Warren> warren;
  if (burrow == "")
    warren = cottontail::Warren::make(simple, &error);
  else
    warren = cottontail::Warren::make(simple, burrow, &error);
  if (warren == nullptr) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }
  warren->start();
  std::string docnos;
  std::string id_key = "id";
  if (!warren->get_parameter(id_key, &docnos))
    docnos = "(... <DOCNO> </DOCNO>)";
  std::unique_ptr<cottontail::Hopper> hopper =
      warren->hopper_from_gcl(docnos, &error);
  if (hopper == nullptr) {
    std::cout << program_name << ": " << error << " can't find identifiers\n";
    return 1;
  }
  std::shared_ptr<cottontail::Ranker> rank =
      cottontail::Ranker::from_pipeline(pipeline, warren, &error);
  if (rank == nullptr) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }
  std::ifstream queriesf(queries_filename);
  if (queriesf.fail()) {
    std::cerr << program_name << ": can't open file: " + queries_filename
              << "\n";
    return 1;
  }
  std::string line;
  std::map<std::string, std::string> queries;
  std::vector<std::string> topics;
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
    queries[topic] = query;
    topics.push_back(topic);
  }
  std::mutex output_lock;
  auto solver = [&](size_t i) {
    for (size_t j = i; j < topics.size(); j += threads) {
      std::string topic = topics[j];
      std::string query = queries[topic];
      std::vector<cottontail::RankingResult> ranking = (*rank)(query);
      output_lock.lock();
      if (ranking.size() == 0) {
        std::cerr << program_name << ": no results for topic \"" << topic
                  << "\" (creating a fake one)\n";
        std::cout << topic << " Q0 FAKE 1 1 cottontail\n";
      } else {
        for (size_t i = 0; i < ranking.size(); i++) {
          cottontail::addr p, q;
          hopper->tau(ranking[i].container_p(), &p, &q);
          if (q <= ranking[i].container_q()) {
            std::string docno =
                cottontail::trec_docno(warren->txt()->translate(p, q));
            std::cout << topic << " Q0 " << docno << " " << i + 1 << " "
                      << ranking[i].score() << " cottontail\n";
          }
        }
      }
      std::flush(std::cout);
      output_lock.unlock();
    }
  };
  std::vector<std::thread> workers;
  for (size_t i = 0; i < threads; i++)
    workers.emplace_back(std::thread(solver, i));
  for (auto &worker : workers)
    worker.join();
  warren->end();
  return 0;
}
