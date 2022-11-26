#define ASSERT_EQ(a, b) (assert((a) == (b)))
#define ASSERT_NE(a, b) (assert((a) != (b)))
#define ASSERT_TRUE(a) (assert((a)))
#define ASSERT_FALSE(a) (assert(!(a)))
#define EXPECT_EQ(a, b) (assert((a) == (b)))
#define EXPECT_NE(a, b) (assert((a) != (b)))
#define EXPECT_TRUE(a) (assert((a)))
#define EXPECT_FALSE(a) (assert(!(a)))

#include <cmath>
#define EXPECT_FLOAT_EQ(a, b) assert(fabs((a) - (b)) < 0.00000001)

#include <cstring>
#define EXPECT_STREQ(a, b) (assert(strcmp((a), (b)) == 0))
#define ASSERT_STREQ(a, b) (assert(strcmp((a), (b)) == 0))

#include <fstream>
#include <unistd.h>

#include <memory>
#include <string>

#include "src/cottontail.h"

#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <memory>
#include <string>

#include "src/cottontail.h"

#include "src/fiver.h"

void usage(std::string program_name) {
  std::cerr << "usage: " << program_name << " [--burrow burrow] queries\n";
}

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  std::string burrow = cottontail::DEFAULT_BURROW;
  if (argc == 2 && argv[1] == std::string("--help")) {
    usage(program_name);
    return 0;
  }
  if (argc == 4 &&
      (argv[1] == std::string("-b") || argv[1] == std::string("--burrow"))) {
    burrow = argv[2];
    argc -= 2;
    argv += 2;
  }
  if (argc != 2) {
    usage(program_name);
    return 1;
  }
  std::string queries_filename = argv[1];
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
  std::string error;
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::Warren::make("simple", burrow, &error);
  if (warren == nullptr) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }
  warren->start();
  for (auto &query : queries) {
    cottontail::addr total = 0, t0 = cottontail::now();
    std::cout << "\n" << "Query: " << query.second << "\n";
    std::vector<std::string> terms = warren->tokenizer()->split(query.second);
    std::vector<std::shared_ptr<cottontail::Hopper>> hoppers;
    for (auto &term : terms) {
      std::string tf = "tf:" + warren->stemmer()->stem(term);
      std::cout << tf << "\n";
      cottontail::addr t1 = cottontail::now();
      hoppers.emplace_back(warren->hopper_from_gcl(tf));
      cottontail::addr gap = cottontail::now() - t1;
      std::cout << tf << " (" << gap << "ms)\n";
      total += gap;
    }
    cottontail::addr p = 0, q = 0;
    for (auto &hopper : hoppers) {
      hopper->tau(0, &p, &q);
      std::cout << p << " " << q << "\n";
      if (p > q)
        std::cout << "what?\n";
    }
    std::cout << "Query: " << query.second << " (" << total << ", " << cottontail::now() - t0 << "ms)\n";
#if 0
      for (hopper->tau(0, &p, &q); p < cottontail::maxfinity;
           hopper->tau(p + 1, &p, &q))
        if (p > q)
          std::cout << "what?\n";
#endif
  }
  return 0;
}
