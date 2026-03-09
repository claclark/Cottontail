#include <exception>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "src/cottontail.h"

namespace {

void usage(const std::string &program_name) {
  std::cerr
      << "usage: " << program_name
      << " [--verbose] [--threads n] [--burrow burrow] queries pipeline...\n";
}

constexpr size_t DEFAULT_THREADS = 50;

} // namespace

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  if (argc == 2 && argv[1] == std::string("--help")) {
    usage(program_name);
    return 0;
  }

  bool verbose = false;
  std::string burrow = "";
  size_t threads = DEFAULT_THREADS;
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
        int parsed_threads = std::stoi(argv[2]);
        if (parsed_threads > 0 && parsed_threads < (int)DEFAULT_THREADS)
          threads = parsed_threads;
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
  std::shared_ptr<cottontail::Stats> stats = cottontail::Stats::make(warren, &error);
  if (stats == nullptr) {
    std::cerr << program_name << ": " << error << "\n";
    warren->end();
    return 1;
  }

  std::ifstream queriesf(queries_filename);
  if (queriesf.fail()) {
    std::cerr << program_name << ": can't open file: " + queries_filename
              << "\n";
    warren->end();
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
      warren->end();
      return 1;
    }
    std::string topic = line.substr(0, line.find(separator));
    std::string query = line.substr(line.find(separator));
    if (topic == "" || query == "") {
      std::cerr << program_name << ": weird query input: " << line;
      warren->end();
      return 1;
    }
    queries[topic] = query;
    topics.push_back(topic);
  }

  std::map<std::string, std::vector<std::string>> results;
  cottontail::addr t0 = 0;
  if (verbose) {
    std::cerr << "Release the rankers...\n" << std::flush;
    t0 = cottontail::now();
  }
  if (!cottontail::trec(stats, pipeline, queries, &results, &error, threads)) {
    std::cerr << program_name << ": " << error << "\n";
    warren->end();
    return 1;
  }
  warren->end();
  if (verbose) {
    time_t t1 = cottontail::now();
    std::cerr << "Ranking took: " << (t1 - t0) << " millisecond(s) \n"
              << std::flush;
  }

  std::string runid = "cottontail";
  for (const auto &topic : topics) {
    const std::vector<std::string> &docnos = results[topic];
    if (docnos.size() == 0) {
      if (verbose)
        std::cerr << program_name << ": no results for topic \"" << topic
                  << "\" (creating a fake one)\n";
      std::cout << topic << " Q0 FAKE 1 1 " << runid << "\n";
      continue;
    }
    for (size_t i = 0; i < docnos.size(); i++) {
      size_t rank = i + 1;
      size_t score = docnos.size() + 1 - rank;
      std::cout << topic << " Q0 " << cottontail::trec_docno(docnos[i]) << " "
                << rank << " " << score << " " << runid << "\n";
    }
  }

  return 0;
}
