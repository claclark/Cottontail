#include <iostream>
#include <memory>
#include <sstream>
#include <vector>

#include "linenoise.h"

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
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::Warren::make(burrow, &error);
  if (warren == nullptr) {
    std::cerr << argv[0] << ": " << error << "\n";
    return 1;
  }
  linenoiseHistorySetMaxLen(1000);
  char *input;
  while ((input = linenoise(">> ")) != nullptr) {
    std::string line = input;
    linenoiseFree(input);
    if (line.empty())
      continue;
    linenoiseHistoryAdd(line.c_str());
    auto clean = [](std::string s) {
      for (size_t i = 0; i < s.length(); i++)
        if (s[i] == '\n' || s[i] == '\r')
          s[i] = ' ';
      return cottontail::json_translate(s);
    };

    warren->start();
    std::shared_ptr<cottontail::Txt> txt = warren->txt();
    std::unique_ptr<cottontail::Hopper> fluffy =
      warren->hopper_from_gcl(line, &error);
    if (fluffy == nullptr) {
      std::cerr << error << "\n";
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
        if (v == 0.0) {
          std::cout << "(" << p << "," << q << "): ";
        } else {
          cottontail::addr n = v;
          if (v - n > 0.0)
            std::cout << "(" << p << "," << q << ")" << v << ": ";
          else
            std::cout << "(" << p << "," << q << ")" << n << ": ";
        }
      }
      cottontail::addr const WINDOW = 128;
      if (q - p <= WINDOW) {
        std::string b = clean(txt->translate(p, q));
        std::cout << b << "\n";
      } else {
        std::string a = clean(txt->translate(p, p + WINDOW / 2));
        std::string c = clean(txt->translate(q - WINDOW / 2, q));
        std::cout << a << "... " << c << "\n";
      }
    }
    warren->end();
  }
  return 0;
}
