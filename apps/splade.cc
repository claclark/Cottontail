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
#include "src/json.h"

void usage(std::string program_name) {
  std::cerr << "usage: " << program_name
            << " [--threads n] [--burrow burrow] queries\n";
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
  if (argc < 2) {
    usage(program_name);
    return 1;
  }
  if (argc > 2 &&
      (argv[1] == std::string("-b") || argv[1] == std::string("--burrow"))) {
    burrow = argv[2];
    argc -= 2;
    argv += 2;
  }
  if (argc < 2) {
    usage(program_name);
    return 1;
  }
  std::string queries_filename = argv[1];
  std::string runid = "cottontail";
  std::string error;
  std::shared_ptr<cottontail::Warren> warren;
  warren = cottontail::Warren::make("simple", burrow, &error);
  if (warren == nullptr) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }
  warren->start();
  std::string id_query;
  std::string id_key = "id";
  if (!warren->get_parameter(id_key, &id_query)) {
    std::cerr << program_name << ": no identifiers in warren\n";
    return 1;
  }
  std::unique_ptr<cottontail::Hopper> hopper =
      warren->hopper_from_gcl(id_query, &error);
  if (hopper == nullptr) {
    std::cout << program_name << ": error parsing identifier query:" << error
              << "\n";
    return 1;
  }
  std::ifstream queriesf(queries_filename);
  if (queriesf.fail()) {
    std::cerr << program_name << ": can't open queries: " + queries_filename
              << "\n";
    return 1;
  }
  std::map<std::string, std::map<std::string, cottontail::fval>> queries;
  std::vector<std::string> topics;
  std::string line;
  unsigned number = 0;
  while (std::getline(queriesf, line)) {
    number++;
    json j;
    try {
      j = json::parse(line);
    } catch (json::parse_error &e) {
      std::cerr << program_name << ": can't parse JSON: " << number << ": "
                << queries_filename << "\n";
      continue;
    }
    if (!j["qid"].is_string()) {
      std::cerr << program_name << ": missing qid: " << number << ": "
                << queries_filename << "\n";
      continue;
    }
    std::string topic = j["qid"];
    topics.push_back(topic);
    if (!j["splade_vector"].is_object()) {
      std::cerr << program_name << ": missing splade vector: " << number << ": "
                << queries_filename << "\n";
      continue;
    }
    for (auto &element : j["splade_vector"].items()) {
      cottontail::fval weight = 0;
      if (element.value().is_number_float())
        weight = element.value();
      if (weight > 0.0) {
        std::string term = element.key();
        queries[topic][term] = weight;
      }
    }
  }
  std::mutex output_lock;
  auto solver = [&](size_t i) {
    for (size_t j = i; j < topics.size(); j += threads) {
      std::map<std::string, cottontail::fval> parameters;
      parameters["splade:depth"] = 10;
      std::string topic = topics[j];
      std::vector<cottontail::RankingResult> ranking =
          product_ranking(warren, queries[topic], parameters, "splade", true);
      output_lock.lock();
      if (ranking.size() == 0) {
        std::cerr << program_name << ": no results for topic \"" << topic
                  << "\" (creating a fake one)\n";
        std::cout << topic << " Q0 FAKE 1 1 cottontail\n";
      } else {
        for (size_t i = 0; i < ranking.size(); i++) {
          cottontail::addr p, q;
          hopper->tau(ranking[i].container_p(), &p, &q);
            std::string docno =
                cottontail::trec_docno(warren->txt()->translate(p, q));
            std::cout << topic << " Q0 " << docno << " " << i + 1 << " "
                      << ranking[i].score() << " cottontail\n";
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
