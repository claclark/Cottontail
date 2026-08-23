#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "meadowlark/meadowlark.h"
#include "src/cottontail.h"

void usage(std::string program_name) {
  std::cerr << "usage: " << program_name
            << " [--meadow meadow] [--key value | --key=value ...] "
            << "query name[:tag] [file ...]\n";
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

  // Remaining args: query name[:tag] [file ...]
  if (argc < 2) {
    usage(program_name);
    return 1;
  }

  std::string query = argv[0];
  std::string forager = argv[1];
  std::string name = forager;
  std::string tag;
  size_t colon = forager.find(':');
  if (colon != std::string::npos) {
    name = forager.substr(0, colon);
    tag = forager.substr(colon + 1);
  }
  if (name.empty()) {
    usage(program_name);
    return 1;
  }
  std::vector<std::string> filenames;
  for (int i = 2; i < argc; i++)
    filenames.emplace_back(argv[i]);

  std::shared_ptr<cottontail::Warren> warren;

  if (meadow.empty())
    warren = cottontail::meadowlark::open_meadow(&error);
  else
    warren = cottontail::meadowlark::open_meadow(meadow, &error);

  if (warren == nullptr) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }

  bool okay;
  if (parameters.empty())
    okay = cottontail::meadowlark::forage_all(warren, filenames, query, name,
                                              tag, &error);
  else
    okay = cottontail::meadowlark::forage_all(
        warren, filenames, query, name, tag, parameters, &error);
  if (!okay) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }

  return 0;
}
