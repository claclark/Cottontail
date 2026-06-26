#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <readline/history.h>
#include <readline/readline.h>

#include "src/cottontail.h"

namespace {

constexpr cottontail::addr WINDOW_TOKENS = 200;
constexpr size_t DEPTH = 20;
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

std::string clean_passage(std::string text) {
  std::replace(text.begin(), text.end(), '\n', ' ');
  std::replace(text.begin(), text.end(), '\r', ' ');
  return text;
}

std::string translate_passage(std::shared_ptr<cottontail::Warren> warren,
                              cottontail::addr p, cottontail::addr q) {
  return clean_passage(warren->txt()->translate(p, q));
}

std::string highlighted_passage(std::shared_ptr<cottontail::Warren> warren,
                                cottontail::addr start, cottontail::addr end,
                                cottontail::addr highlight_start,
                                cottontail::addr highlight_end) {
  if (highlight_start < start || highlight_end > end ||
      highlight_start > highlight_end)
    return translate_passage(warren, start, end);
  std::string text;
  if (start < highlight_start)
    text += translate_passage(warren, start, highlight_start - 1);
  text += "<b>";
  text += translate_passage(warren, highlight_start, highlight_end);
  text += "</b>";
  if (highlight_end < end)
    text += translate_passage(warren, highlight_end + 1, end);
  return text;
}

std::string passage(std::shared_ptr<cottontail::Warren> warren,
                    const cottontail::RankingResult &result) {
  cottontail::addr cp = result.container_p();
  cottontail::addr cq = result.container_q();
  cottontail::addr p = result.p();
  cottontail::addr q = result.q();
  if (cq - cp + 1 <= WINDOW_TOKENS)
    return highlighted_passage(warren, cp, cq, p, q);
  if (q - p + 1 >= WINDOW_TOKENS)
    return highlighted_passage(warren, p, q, p, q);
  cottontail::addr extra = WINDOW_TOKENS - (q - p + 1);
  cottontail::addr start = p - extra / 2;
  cottontail::addr end = q + extra - extra / 2;
  if (start < cp) {
    end += cp - start;
    start = cp;
  }
  if (end > cq) {
    start -= end - cq;
    end = cq;
    if (start < cp)
      start = cp;
  }
  return highlighted_passage(warren, start, end, p, q);
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

  char *line;
  std::vector<cottontail::RankingResult> ranking;
  size_t next_result = 0;
  auto print_next = [&]() {
    if (next_result < ranking.size()) {
      std::cout << passage(warren, ranking[next_result++]) << "\n";
      std::cout.flush();
    }
  };
  while ((line = readline(">> ")) != nullptr) {
    std::string query = line;
    std::free(line);
    if (query.empty()) {
      print_next();
      continue;
    }
    add_history(query.c_str());
    cottontail::addr start = cottontail::now();
    ranking = cottontail::parallel_ssr(warren, query, CONTAINER, DEPTH);
    cottontail::addr elapsed = cottontail::now() - start;
    std::cerr << "Ranking took: " << elapsed << " millisecond(s)\n";
    next_result = 0;
    print_next();
  }

  warren->end();
  return 0;
}
