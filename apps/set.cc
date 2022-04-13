#include <string>

#include "src/cottontail.h"

void usage(std::string program_name) {
  std::cerr << "usage: " << program_name << " [--burrow burrow] key [value]\n";
}

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  if (argc == 2 && argv[1] == std::string("--help")) {
    usage(program_name);
    return 0;
  }
  std::string burrow = cottontail::DEFAULT_BURROW;
  if (argc > 2 &&
      (argv[1] == std::string("-b") || argv[1] == std::string("--burrow"))) {
    burrow = argv[2];
    argc -= 2;
    argv += 2;
  }
  if (argc != 2 && argc != 3) {
    usage(program_name);
    return 1;
  }
  std::string error;
  std::string simple = "simple";
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::Warren::make(simple, burrow, &error);
  if (warren == nullptr) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }
  warren->start();
  std::string key = argv[1];
  std::string value;
  if (argc == 2) {
    if (!warren->get_parameter(key, &value, &error)) {
      std::cerr << program_name << ": " << error << "\n";
      return 1;
    }
  } else {
    value = argv[2];
    if (!warren->set_parameter(key, value, &error)) {
      std::cerr << program_name << ": " << error << "\n";
      return 1;
    }
  }
  std::cout << key << " = \"" << value << "\"\n";
  warren->end();
  return 0;
}
