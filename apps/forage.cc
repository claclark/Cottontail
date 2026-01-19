#include <memory>
#include <string>

#include "meadowlark/forager.h"
#include "meadowlark/meadowlark.h"
#include "src/cottontail.h"

void usage(std::string program_name) {
  std::cerr << "usage: " << program_name
            << " [--meadow meadow] [OPTONS] query [name [tag]]\n";
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
  if (argc < 2 || argc > 4) {
    usage(program_name);
    return 1;
  }
  const std::map<std::string, std::string> parameters;
  std::string query = argv[1];
  std::string name;
  std::string tag;
  if (argc > 2) {
    name = argv[2];
    if (argc > 3)
      tag = argv[3];
  }
  std::shared_ptr<cottontail::Warren> warren;
  if (meadow == "")
    warren = cottontail::meadowlark::open_meadow(&error);
  else
    warren = cottontail::meadowlark::open_meadow(meadow, &error);
  if (warren == nullptr) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }
  if (!cottontail::meadowlark::forage(warren, query, name, tag, parameters,
                                      &error)) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }
  return 0;
}
