#include <algorithm>
#include <exception>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <vector>

#include "src/cottontail.h"

namespace {

void usage(const std::string &program_name) {
  std::cerr
      << "usage: " << program_name
      << " [--verbose] [--threads n] [--burrow burrow]"
      << " [--statistics name[:recipe]] queries pipeline...\n";
}

} // namespace

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  if (argc == 2 && argv[1] == std::string("--help")) {
    usage(program_name);
    return 0;
  }

  bool verbose = false;
  std::string burrow = "";
  std::string stats_name = "";
  std::string stats_recipe = "";
  size_t threads = 0;
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
        std::string argument = argv[2];
        if (argument.size() == 0 || argument[0] == '-')
          throw std::exception();
        size_t end = 0;
        unsigned long long parsed_threads = std::stoull(argument, &end);
        if (end != argument.size() ||
            parsed_threads > std::numeric_limits<size_t>::max())
          throw std::exception();
        threads = parsed_threads;
      } catch (std::exception &e) {
        std::cerr << program_name << ": bad thread count: " << argv[2] << "\n";
        return 1;
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
    if (argc > 2 &&
        (argv[1] == std::string("-s") || argv[1] == std::string("--stats") ||
         argv[1] == std::string("--statistics"))) {
      std::string statistics = argv[2];
      size_t colon = statistics.find(":");
      if (colon == std::string::npos) {
        stats_name = statistics;
        stats_recipe = "";
      } else {
        stats_name = statistics.substr(0, colon);
        stats_recipe = statistics.substr(colon + 1);
      }
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

  std::map<std::string, std::vector<std::string>> results;
  cottontail::addr t0 = 0;
  if (verbose) {
    std::cerr << "Release the rankers...\n" << std::flush;
    t0 = cottontail::now();
  }
  if (!cottontail::trec(warren, stats_name, stats_recipe, pipeline, queries,
                        &results, &error, threads)) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }
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
