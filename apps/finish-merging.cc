#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include "src/cottontail.h"

namespace {

void usage(const std::string &program_name) {
  std::cerr << "usage: " << program_name << " burrow [burrow...]\n";
}

bool shard_count(const std::string &burrow, size_t *count,
                 std::string *error) {
  *count = 0;
  std::error_code ec;
  std::filesystem::directory_iterator entry(burrow, ec);
  std::filesystem::directory_iterator end;
  if (ec) {
    *error = "Can't list burrow: " + burrow + ": " + ec.message();
    return false;
  }
  while (entry != end) {
    std::string name = entry->path().filename().string();
    if (name.rfind("fiver.", 0) == 0 || name.rfind("hazel.", 0) == 0)
      ++*count;
    entry.increment(ec);
    if (ec) {
      *error = "Can't list burrow: " + burrow + ": " + ec.message();
      return false;
    }
  }
  return true;
}

} // namespace

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  if (argc == 2 && argv[1] == std::string("--help")) {
    usage(program_name);
    return 0;
  }
  if (argc < 2) {
    usage(program_name);
    return 1;
  }

  std::vector<std::string> burrows;
  std::vector<std::shared_ptr<cottontail::Warren>> warrens;
  for (int i = 1; i < argc; i++) {
    std::string burrow = argv[i];
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
    std::string error;
    std::shared_ptr<cottontail::Warren> warren =
        cottontail::Warren::make(burrow, &error);
    if (warren == nullptr) {
      std::cerr << program_name << ": " << error << "\n";
      return 1;
    }
    burrows.push_back(burrow);
    warrens.push_back(warren);
  }

  for (;;) {
    bool finished = true;
    for (const auto &burrow : burrows) {
      size_t count;
      std::string error;
      if (!shard_count(burrow, &count, &error)) {
        std::cerr << program_name << ": " << error << "\n";
        return 1;
      }
      if (count != 1)
        finished = false;
    }
    if (finished)
      return 0;
    std::this_thread::sleep_for(std::chrono::seconds(10));
  }
}
