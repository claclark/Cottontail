#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <queue>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "src/cottontail.h"

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  std::string error;
  std::string simple = "simple";
  std::string burrow = "/data/hdd3/claclark/TREC-DL-2021/the.burrow";
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::Warren::make(simple, burrow, &error);
  warren->start();
  std::string line;
  while (std::getline(std::cin, line)) {
    std::vector<std::string> tokens;
    std::istringstream iss(line);
    std::string token;
    while (std::getline(iss, token, '\t')) // but we can specify a different one
      tokens.push_back(token);
    std::string docno = tokens[2];
    std::string query = "(>> :paragraph \"" + docno + "\")";
    std::cout << query << "\n";
    auto clean = [](std::string s) {
      for (size_t i = 0; i < s.length(); i++)
        if (s[i] == '\n' || s[i] == '\t')
          s[i] = ' ';
      return s;
    };
    std::unique_ptr<cottontail::Hopper> hopper = warren->hopper_from_gcl(query);
    cottontail::addr p, q;
    hopper->tau(0, &p, &q);
    std::string text = clean(warren->txt()->translate(p + 4, q));
    std::cout << line << "\t" << text << "\n";
  }
  warren->end();
  return 0;
}
