#include <iostream>
#include <memory>
#include <sstream>
#include <vector>

#include <readline/history.h>
#include <readline/readline.h>

#include "src/cottontail.h"

void usage(std::string program_name) {
  std::cerr << "usage: " << program_name
            << " [--burrow burrow] [--addr|--fval]\n";
}

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  if (argc == 2 && argv[1] == std::string("--help")) {
    usage(program_name);
    return 0;
  }
  bool report_addr = true;
  std::string burrow = cottontail::DEFAULT_BURROW;
  if (argc > 2 &&
      (argv[1] == std::string("-b") || argv[1] == std::string("--burrow"))) {
    burrow = argv[2];
    argc -= 2;
    argv += 2;
  }
  if (argc == 2) {
    if (argv[1] == std::string("-a") || argv[1] == std::string("--addr")) {
      report_addr = true;
    } else if (argv[1] == std::string("-f") ||
               argv[1] == std::string("--fval")) {
      report_addr = false;
    } else {
      usage(program_name);
      return 1;
    }
  } else if (argc != 1) {
    usage(program_name);
    return 1;
  }
  std::string error;
#if 0
  std::string simple = "simple";
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::Warren::make(simple, burrow, &error);
#endif
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::Warren::make(burrow, &error);
  if (warren == nullptr) {
    std::cerr << argv[0] << ": " << error << "\n";
    return 1;
  }
  char *line;
  while ((line = readline(">> ")) != nullptr) {
    if (strlen(line) > 0) {
      add_history(line);
    } else {
      continue;
    }
    auto thing = [](std::string s) {
      for (size_t i = 0; i < s.length(); i++)
        if (s[i] == '\n')
          s[i] = ' ';
      return s;
    };

    warren->start();
    std::shared_ptr<cottontail::Featurizer> featurizer = warren->featurizer();
    std::shared_ptr<cottontail::Tokenizer> tokenizer = warren->tokenizer();
    std::shared_ptr<cottontail::Txt> txt = warren->txt();
    std::shared_ptr<cottontail::Idx> idx = warren->idx();
    std::shared_ptr<cottontail::gcl::SExpression> expr =
        cottontail::gcl::SExpression::from_string(line, &error);
    if (expr == nullptr) {
      std::cerr << error << "\n";
      free(line);
      warren->end();
      continue;
    } else {
      free(line);
    }
    expr = expr->expand_phrases(tokenizer);
    std::unique_ptr<cottontail::Hopper> fluffy =
        expr->to_hopper(featurizer, idx);
    if (fluffy == nullptr) {
      std::cerr << "implementation failure\n";
      warren->end();
      continue;
    }
    cottontail::addr k = cottontail::minfinity + 1, p, q;
    cottontail::fval v;
    unsigned j = 0;
    for (fluffy->tau(k, &p, &q, &v); p < cottontail::maxfinity && j < 24;
         fluffy->tau(k, &p, &q, &v)) {
      j++;
      k = p + 1;
      if (report_addr) {
        cottontail::addr n = cottontail::fval2addr(v);
        if (n == 0)
          std::cout << "(" << p << "," << q << "): ";
        else
          std::cout << "(" << p << "," << q << ")" << n << ": ";
      } else {
        if (v == 0.0)
          std::cout << "(" << p << "," << q << "): ";
        else
          std::cout << "(" << p << "," << q << ")" << v << ": ";
      }
      cottontail::addr const WINDOW = 128;
      if (q - p <= WINDOW) {
        std::string b = thing(txt->translate(p, q));
        std::cout << b << "\n";
      } else {
        std::string a = thing(txt->translate(p, p + WINDOW / 2));
        std::string c = thing(txt->translate(q - WINDOW / 2, q));
        std::cout << a << "... " << c << "\n";
      }
    }
    warren->end();
  }
  return 0;
}
