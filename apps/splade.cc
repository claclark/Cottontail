#include <exception>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "src/cottontail.h"
#include "src/nlohmann.h"

namespace {

void usage(const std::string &program_name) {
  std::cerr << "usage: " << program_name
            << " [--verbose] [--threads n] [--burrow burrow]"
            << " [--container query] [--id query] [--prefix prefix]"
            << " [--depth n] queries.jsonl\n";
}

} // namespace

int main(int argc, char **argv) {
  const std::string program_name = argv[0];
  bool verbose = false;
  size_t threads = 0;
  std::string burrow, queries_filename;
  std::map<std::string, std::string> parameters = {{"depth", "10"}};
  bool options = true;
  for (int i = 1; i < argc; i++) {
    std::string argument = argv[i];
    if (options && argument == "--help") {
      usage(program_name);
      return 0;
    }
    if (options && argument == "--") {
      options = false;
      continue;
    }
    if (options && (argument == "--verbose" || argument == "-v")) {
      verbose = true;
      continue;
    }
    if (options && !argument.empty() && argument[0] == '-') {
      size_t equal = argument.find('=');
      std::string key = argument.substr(0, equal);
      if (key != "--burrow" && key != "--meadow" && key != "-b" &&
          key != "--threads" && key != "-t" && key != "--container" &&
          key != "--id" && key != "--prefix" && key != "--depth") {
        std::cerr << program_name << ": unknown option: " << key << "\n";
        usage(program_name);
        return 1;
      }
      std::string value;
      if (equal != std::string::npos) {
        value = argument.substr(equal + 1);
      } else if (i + 1 < argc) {
        value = argv[++i];
      } else {
        std::cerr << program_name << ": missing value for " << key << "\n";
        return 1;
      }
      if (key == "--burrow" || key == "--meadow" || key == "-b") {
        burrow = value;
      } else if (key == "--threads" || key == "-t") {
        if (value.empty() || value.find_first_not_of("0123456789") !=
                                 std::string::npos) {
          std::cerr << program_name << ": bad thread count: " << value << "\n";
          return 1;
        }
        try {
          unsigned long long count = std::stoull(value);
          if (count > std::numeric_limits<size_t>::max())
            throw std::exception();
          threads = count;
        } catch (const std::exception &) {
          std::cerr << program_name << ": bad thread count: " << value << "\n";
          return 1;
        }
      } else {
        parameters[key.substr(2)] = value;
      }
    } else if (queries_filename.empty()) {
      queries_filename = argument;
    } else {
      usage(program_name);
      return 1;
    }
  }
  if (queries_filename.empty()) {
    usage(program_name);
    return 1;
  }
  std::ifstream input(queries_filename);
  if (!input) {
    std::cerr << program_name << ": can't open file: " << queries_filename << "\n";
    return 1;
  }
  std::map<std::string, std::map<std::string, cottontail::fval>> queries;
  std::vector<std::string> topics;
  std::string line;
  size_t number = 0;
  while (std::getline(input, line)) {
    number++;
    try {
      json record = json::parse(line);
      if (!record.is_object() || !record.contains("qid") ||
          !record["qid"].is_string() || !record.contains("splade_vector") ||
          !record["splade_vector"].is_object())
        throw std::runtime_error("expected string qid and object splade_vector");
      std::string topic = record["qid"].get<std::string>();
      if (topic.empty() || queries.find(topic) != queries.end())
        throw std::runtime_error("empty or duplicate qid");
      std::map<std::string, cottontail::fval> query;
      for (const auto &element : record["splade_vector"].items()) {
        if (!element.value().is_number())
          throw std::runtime_error("nonnumeric query weight: " + element.key());
        query[element.key()] = element.value().get<cottontail::fval>();
      }
      queries[topic] = std::move(query);
      topics.push_back(topic);
    } catch (const std::exception &e) {
      std::cerr << program_name << ": " << queries_filename << ":" << number
                << ": " << e.what() << "\n";
      return 1;
    }
  }
  if (!input.eof()) {
    std::cerr << program_name << ": read error: " << queries_filename << "\n";
    return 1;
  }

  std::string error;
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::Warren::make(burrow, &error);
  if (warren == nullptr) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }
  std::map<std::string, std::vector<std::string>> results;
  cottontail::addr ranking_time = 0;
  if (verbose)
    std::cerr << "Release the rankers...\n" << std::flush;
  if (!cottontail::splade(warren, queries, parameters, &results, &error, threads,
                          &ranking_time)) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }
  if (verbose)
    std::cerr << "Ranking took: " << ranking_time << " millisecond(s)\n";
  for (const auto &topic : topics) {
    const auto &docnos = results[topic];
    if (docnos.empty()) {
      if (verbose)
        std::cerr << program_name << ": no results for topic \"" << topic
                  << "\" (creating a fake one)\n";
      std::cout << topic << " Q0 FAKE 1 1 cottontail\n";
    } else {
      for (size_t i = 0; i < docnos.size(); i++)
        std::cout << topic << " Q0 " << cottontail::trec_docno(docnos[i]) << " "
                  << i + 1 << " " << docnos.size() - i << " cottontail\n";
    }
  }
  return 0;
}
