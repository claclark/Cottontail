#include "apps/walk.h"

#include <exception>
#include <filesystem>
#include <string>
#include <vector>

namespace cottontail {
namespace {

void try_adding_file(const std::filesystem::path &p,
                     std::vector<std::string> *text) {
  text->push_back(p.string());
}

void try_adding_directory(const std::filesystem::path &p,
                          std::vector<std::string> *text) {
  for (const std::filesystem::directory_entry &x :
       std::filesystem::directory_iterator(p)) {
    std::filesystem::file_status status = x.symlink_status();
    if (std::filesystem::is_regular_file(status))
      try_adding_file(x.path(), text);
    else if (std::filesystem::is_directory(status))
      try_adding_directory(x.path(), text);
  }
}
} // namespace

bool walk_filesystem(char *name, std::vector<std::string> *text) {
  std::filesystem::path p(name);
  try {
    std::filesystem::file_status status = std::filesystem::symlink_status(p);
    if (std::filesystem::is_regular_file(status)) {
      try_adding_file(p, text);
      return true;
    } else if (std::filesystem::is_directory(status)) {
      try_adding_directory(p, text);
      return true;
    } else {
      return false;
    }
  } catch (std::exception &e) {
    return false;
  }
}
} // namespace cottontail
