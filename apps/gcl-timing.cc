#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "src/cottontail.h"

namespace {

constexpr cottontail::addr SNIPPET_TOKENS = 100;
const std::vector<std::string> QUERIES = {
    "(^ black bear attacks)",
    "(^ black bear attack)",
    "(^ black bear attacks)",
    "(^ prevent black bear attacks)",
    "(^ preventing black bear attacks)",
    "(^ avoid black bear attacks)",
    "(^ bear spray)",
    "(^ bear spray black bears)",
    "(^ bear resistant containers)",
    "(^ bear proof garbage)",
    "(^ secure garbage bears)",
    "(^ keep food away from bears)",
    "(^ store food away from bears)",
    "(^ avoid surprising bears)",
    "(^ make noise bears)",
};
const std::string CONTAINER = ":";

void usage(const std::string &program_name) {
  std::cerr << "usage: " << program_name << " [--burrow burrow]\n";
}

std::shared_ptr<cottontail::Warren> activate(const std::string &burrow,
                                             std::string *error) {
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::Warren::make(burrow, error);
  if (warren != nullptr)
    warren->start();
  return warren;
}

std::string snippet(std::shared_ptr<cottontail::Warren> warren,
                    const cottontail::RankingResult &result) {
  cottontail::addr p = result.container_p();
  cottontail::addr q = result.container_q();
  if (p <= q && q - p + 1 > SNIPPET_TOKENS)
    q = p + SNIPPET_TOKENS - 1;
  std::string text = warren->txt()->translate(p, q);
  std::replace(text.begin(), text.end(), '\n', ' ');
  std::replace(text.begin(), text.end(), '\r', ' ');
  return text;
}

} // namespace

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  std::string burrow = cottontail::DEFAULT_BURROW;
  for (int arg = 1; arg < argc; arg++) {
    std::string option = argv[arg];
    if (option == "--help") {
      usage(program_name);
      return 0;
    } else if (option == "-b" || option == "--burrow") {
      if (arg + 1 >= argc) {
        usage(program_name);
        return 1;
      }
      burrow = argv[++arg];
    } else {
      usage(program_name);
      return 1;
    }
  }

  std::string error;
  std::shared_ptr<cottontail::Warren> warren = activate(burrow, &error);
  if (warren == nullptr) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }

  for (const auto &query : QUERIES) {
    std::cout << "QUERY: " << query << "\n";
    std::cerr << "QUERY: " << query << "\n";
    std::cerr << "Ranking started...\n";
    cottontail::addr start = cottontail::now();
    std::vector<cottontail::RankingResult> ranking =
        cottontail::parallel_ssr(warren, query, CONTAINER, 10);
    cottontail::addr elapsed = cottontail::now() - start;
    std::cerr << "Ranking took: " << elapsed << " millisecond(s)\n";
    for (const auto &result : ranking)
      std::cout << snippet(warren, result) << "\n";
  }

  warren->end();
  return 0;
}
