#include <iostream>
#include <memory>
#include <regex>
#include <string>
#include <vector>

#include "src/cottontail.h"

void usage(std::string program_name) {
  std::cerr << "usage: " << program_name << " [--burrow burrow]\n";
}

std::string strip(std::string s) {
  std::string t;
  if (s.size() == 0)
    return "";
  size_t i = 0;
  if (s[i] == '\"')
    i++;
  for (; i < s.length(); i++)
    if (s[i] == '\t' || s[i] == '\n' || s[i] == '\r')
      t += ' ';
    else
      t += s[i];
  if (t.back() == '"')
    t.pop_back();
  return t;
}

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  if (argc == 2 && argv[1] == std::string("--help")) {
    usage(program_name);
    return 0;
  }
  std::string burrow = cottontail::DEFAULT_BURROW;
  if (argc == 3 &&
      (argv[1] == std::string("-b") || argv[1] == std::string("--burrow"))) {
    burrow = argv[2];
    argc -= 2;
    argv += 2;
  }
  std::string error;
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::Warren::make(burrow, &error);
  if (warren == nullptr) {
    std::cerr << argv[0] << ": " << error << "\n";
    return 1;
  }
  warren->start();
  std::unique_ptr<cottontail::Hopper> title = warren->hopper_from_gcl("title:");
  std::unique_ptr<cottontail::Hopper> segment =
      warren->hopper_from_gcl("segment:");
  std::string line;
  while (std::getline(std::cin, line)) {
    std::vector<std::string> fields = cottontail::split(line);
    if (fields.size() < 2) {
      std::cerr << argv[0] << ": format error: " << line << "\n";
      return 1;
    }
    std::string topic = fields[0];
    std::string docno;
    if (fields.size() > 2)
      docno = fields[2];
    else
      docno = fields[1];
    std::string query = "(>> : \"" + docno + "\")";
    std::unique_ptr<cottontail::Hopper> item = warren->hopper_from_gcl(query);
    cottontail::addr p, q;
    item->tau(0, &p, &q);
    if (p == cottontail::maxfinity) {
      std::cerr << argv[0] << "item not found: " << docno << "\n";
      return 1;
    }
    std::string passage;
    cottontail::addr p0, q0;
    title->tau(p, &p0, &q0);
    if (q0 > q)
      std::cerr << argv[0] << "no title in: " << docno << "\n";
    else
      passage = strip(warren->txt()->translate(p0, q0));
    segment->tau(p, &p0, &q0);
    if (q0 > q) {
      std::cerr << argv[0] << "no segment in: " << docno << "\n";
    } else {
      if (passage != "")
        passage += ". ";
      passage += strip(warren->txt()->translate(p0, q0));
    }
    if (passage == "")
      std::cerr << argv[0] << "no content in: " << docno << "\n";
    else
      std::cout << topic << "\t" << docno << "\t" << passage << "\n"
                << std::flush;
  }
  warren->end();
  return 0;
}
