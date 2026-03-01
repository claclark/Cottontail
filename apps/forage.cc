#include <iostream>
#include <map>
#include <memory>
#include <string>

#include "meadowlark/forager.h"
#include "meadowlark/meadowlark.h"
#include "src/cottontail.h"

void usage(std::string program_name) {
  std::cerr << "usage: " << program_name
            << " [--meadow meadow] [--key value | --key=value ...] "
            << "query [name [tag]]\n";
}

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  std::string error;

  // Drop program name
  argc--;
  argv++;

  if (argc == 1 && std::string(argv[0]) == "--help") {
    usage(program_name);
    return 0;
  }

  if (argc <= 0)
    return 0;

  std::string meadow;
  std::map<std::string, std::string> parameters;

  // Parse leading options
  while (argc > 0 && std::string(argv[0]).rfind("--", 0) == 0) {
    std::string arg = argv[0];
    std::string key;
    std::string value;

    std::size_t eq = arg.find('=');

    if (eq != std::string::npos) {
      // --key=value form
      key = arg.substr(2, eq - 2);
      value = arg.substr(eq + 1);

      argc--;
      argv++;
    } else {
      // --key value form
      if (argc < 2) {
        usage(program_name);
        return 1;
      }

      key = arg.substr(2);
      value = argv[1];

      argc -= 2;
      argv += 2;
    }

    if (key == "meadow") {
      meadow = value;
    } else {
      parameters[key] = value;
    }
  }

  // Remaining args: query [name [tag]]
  if (argc < 1 || argc > 3) {
    usage(program_name);
    return 1;
  }

  std::string query = argv[0];
  std::string name;
  std::string tag;

  if (argc > 1)
    name = argv[1];
  if (argc > 2)
    tag = argv[2];

  std::shared_ptr<cottontail::Warren> warren;

  if (meadow.empty())
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
