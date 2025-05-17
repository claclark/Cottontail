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
  std::cerr
      << "usage: " << program_name
      << " [--verbose] [--threads n] [--burrow burrow] queries pipeline...\n";
}

constexpr int THREADS = 50;

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  if (argc == 2 && argv[1] == std::string("--help")) {
    usage(program_name);
    return 0;
  }
  bool verbose = false;
  std::string burrow = "";
  size_t threads = THREADS;
  while (argc > 1) {
    if (argv[1] == std::string("-v") || argv[1] == std::string("--verbose")) {
      verbose = true;
      --argc;
      argv++;
      continue;
    }
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
      continue;
    }
    if (argc > 2 &&
        (argv[1] == std::string("-b") || argv[1] == std::string("--burrow"))) {
      burrow = argv[2];
      argc -= 2;
      argv += 2;
      continue;
    }
    break;
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
  std::string error;
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::Warren::make(burrow, &error);
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
  warren->end();
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
  std::string runid = "cottontail";
  auto solver = [&](size_t i) {
    std::string error;
    std::shared_ptr<cottontail::Warren> larren = warren->clone(&error);
    if (larren == nullptr) {
      output_lock.lock();
      std::cerr << program_name << ": " << error << "\n" << std::flush;
      output_lock.unlock();
      return;
    }
    larren->start();
    std::string id_key = "id";
    std::unique_ptr<cottontail::Hopper> hopper =
        larren->hopper_from_gcl(docnos, &error);
    if (hopper == nullptr) {
      output_lock.lock();
      std::cout << program_name << ": " << error << " can't find identifiers\n"
                << std::flush;
      output_lock.unlock();
      larren->end();
      return;
    }
    std::shared_ptr<cottontail::Ranker> rank =
        cottontail::Ranker::from_pipeline(pipeline, larren, &error);
    if (rank == nullptr) {
      output_lock.lock();
      std::cerr << program_name << ": " << error << "\n" << std::flush;
      output_lock.unlock();
      larren->end();
      return;
    }
    for (size_t j = i; j < topics.size(); j += threads) {
      std::string topic = topics[j];
      std::string query = queries[topic];
      std::vector<cottontail::RankingResult> ranking = (*rank)(query);
      if (ranking.size() == 0) {
        output_lock.lock();
        if (verbose)
          std::cerr << program_name << ": no results for topic \"" << topic
                    << "\" (creating a fake one)\n";
        std::cout << topic << " Q0 FAKE 1 1 cottontail\n";
        output_lock.unlock();
      } else {
        for (size_t i = 0; i < ranking.size(); i++) {
          cottontail::addr p, q;
          hopper->tau(ranking[i].container_p(), &p, &q);
          if (q <= ranking[i].container_q()) {
            std::string docno =
                cottontail::trec_docno(larren->txt()->translate(p, q));
            output_lock.lock();
            std::cout << topic << " Q0 " << docno << " " << i + 1 << " "
                      << ranking[i].score() << " cottontail\n";
            output_lock.unlock();
          }
        }
      }
    }
    larren->end();
  };
  cottontail::addr t0 = 0;
  if (verbose) {
    std::cerr << "Release the rankers...\n" << std::flush;
    t0 = cottontail::now();
  }
  std::vector<std::thread> workers;
  for (size_t i = 0; i < threads; i++)
    workers.emplace_back(std::thread(solver, i));
  for (auto &worker : workers)
    worker.join();
  std::flush(std::cout);
  if (verbose) {
    time_t t1 = cottontail::now();
    std::cerr << "Ranking took: " << (t1 - t0) << " millisecond(s) \n" << std::flush;
  }
  return 0;
}
