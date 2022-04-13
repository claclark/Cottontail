#include <iostream>
#include <string>

#include "src/cottontail.h"
#include "src/fastid_txt.h"

void usage(std::string program_name) {
  std::cerr << "usage: " << program_name << " [--burrow burrow]\n";
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
  } else if (argc > 1) {
    usage(program_name);
    return 1;
  }
  std::string simple = "simple";
  std::string error;
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::Warren::make(simple, burrow, &error);
  if (warren == nullptr) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }
  warren->start();
  if (!fastid(warren, &error)) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }
  warren->end();
  return 0;
}
