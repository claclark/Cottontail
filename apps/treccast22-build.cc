#include <iostream>

#include "apps/collection.h"
#include "src/cottontail.h"

void usage(std::string program_name) {
  std::cerr << "usage: " << program_name
            << " [--burrow burrow] jsonl-file...\n";
}

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  std::string burrow = cottontail::DEFAULT_BURROW;
  if (argc == 2 && argv[1] == std::string("--help")) {
    usage(program_name);
    return 0;
  }
  if (argc > 2 &&
      (argv[1] == std::string("-b") || argv[1] == std::string("--burrow"))) {
    burrow = argv[2];
    argc -= 2;
    argv += 2;
  }
  if (argc == 1) {
    usage(program_name);
    return 1;
  }
  std::string error;
  std::shared_ptr<cottontail::Working> working =
      cottontail::Working::mkdir(burrow, &error);
  if (working == nullptr) {
    std::cerr << program_name << ": " << error << "\n";
    return -1;
  }
  std::string options = "tokenizer:noxml idx:fvalue_compressor:tfdf";
  std::shared_ptr<cottontail::Builder> builder =
      cottontail::SimpleBuilder::make(working, options, &error);
  if (builder == nullptr) {
    std::cerr << program_name << ": " << error << "\n";
    return -1;
  }
  for (int i = 1; i < argc; i++) {
    std::string location = argv[i];
    std::cout << location << "\n";
    if (!collection_CAsT2022_preprocessed(location, builder, &error)) {
      std::cerr << program_name << ": " << error << ": building: " << location
                << "\n";
      return -1;
    }
  }
  if (!builder->finalize(&error)) {
    std::cerr << program_name << ": " << error << "\n";
    return -1;
  }
  return 0;
}
