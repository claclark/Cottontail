#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

#include "src/cottontail.h"

namespace {

void usage(const std::string &program_name) {
  std::cerr << "usage: " << program_name
            << " [--verbose] burrow [burrow...]\n";
}

} // namespace

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  if (argc < 2) {
    usage(program_name);
    return 1;
  }

  bool verbose = false;
  std::vector<std::string> burrows;
  for (int i = 1; i < argc; i++) {
    std::string burrow = argv[i];
    if (burrow == "--help") {
      usage(program_name);
      return 0;
    }
    if (burrow == "--verbose") {
      verbose = true;
      continue;
    }
    std::error_code ec;
    std::filesystem::file_status status = std::filesystem::status(burrow, ec);
    if (ec) {
      std::cerr << program_name << ": " << burrow << ": " << ec.message()
                << "\n";
      return 1;
    }
    if (std::filesystem::is_regular_file(status))
      continue;
    if (!std::filesystem::is_directory(status)) {
      std::cerr << program_name << ": not a file or directory: " << burrow
                << "\n";
      return 1;
    }
    burrows.push_back(burrow);
  }
  if (burrows.empty()) {
    usage(program_name);
    return 1;
  }

  for (const auto &burrow : burrows) {
    std::string error;
    if (!cottontail::Bigwig::consolidate(burrow, &error, verbose)) {
      std::cerr << program_name << ": " << burrow << ": " << error << "\n";
      return 1;
    }
  }
  return 0;
}
