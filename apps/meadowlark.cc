#include <memory>
#include <string>

#include "meadowlark/forager.h"
#include "meadowlark/meadowlark.h"
#include "src/cottontail.h"

void usage(std::string program_name) {
  std::cerr << "usage: " << program_name << " [--meadow meadow] [--create]"
            << " [--tsv file | --text file | --json file]...\n";
}

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  std::string error;
  if (argc == 2 && argv[1] == std::string("--help")) {
    usage(program_name);
    return 0;
  }
  if (argc <= 1)
    return 0;
  std::string meadow;
  if (argc > 2 &&
      (argv[1] == std::string("-m") || argv[1] == std::string("--meadow"))) {
    meadow = argv[2];
    argc -= 2;
    argv += 2;
  }
  if (argc <= 1)
    return 0;
  std::shared_ptr<cottontail::Warren> warren;
  if (argv[1] == std::string("-c") || argv[1] == std::string("--create")) {
    if (meadow == "")
      warren = cottontail::meadowlark::create_meadow(&error);
    else
      warren = cottontail::meadowlark::create_meadow(meadow, &error);
    --argc;
    argv++;
  } else {
    if (meadow == "")
      warren = cottontail::meadowlark::open_meadow(&error);
    else
      warren = cottontail::meadowlark::open_meadow(meadow, &error);
  }
  if (warren == nullptr) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }
  while(argc > 2) {
    if (argv[1] == std::string("--tsv")) {
      if (!cottontail::meadowlark::append_tsv(warren, std::string(argv[2]),
                                              &error)) {
        std::cerr << program_name << ": " << error << "\n";
        return 1;
      }
    } else if (argv[1] == std::string("--jsonl")) {
      if (!cottontail::meadowlark::append_jsonl(warren, std::string(argv[2]),
                                              &error)) {
        std::cerr << program_name << ": " << error << "\n";
        return 1;
      }
    } else {
      std::cerr << program_name << ": Invalid argument" << argv[1] << "\n";
      usage(program_name);
      return 1;
    }
   argc -= 2;
   argv += 2;
  }
  if (argc > 1) {
    std::cerr << program_name << ": Invalid argument" << argv[1] << "\n";
    usage(program_name);
    return 1;
  }
  return 0;
}
