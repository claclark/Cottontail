#include "apps/walk.h"

#include <exception>
#include <regex>
#include <string>
#include <vector>

#include <boost/filesystem.hpp>

namespace cottontail {
namespace {

void try_adding_file(const boost::filesystem::path &p,
                     std::vector<std::string> *text) {
  std::regex readme("readme", std::regex_constants::icase);
  if (!std::regex_search(p.string(), readme))
    text->push_back(p.string());
}

void try_adding_directory(const boost::filesystem::path &p,
                          std::vector<std::string> *text) {
  for (const boost::filesystem::directory_entry &x :
       boost::filesystem::directory_iterator(p))
    if (boost::filesystem::is_regular_file(x.path()))
      try_adding_file(x.path(), text);
    else if (boost::filesystem::is_directory(x.path()))
      try_adding_directory(x.path(), text);
}
} // namespace

bool walk_filesystem(char *name, std::vector<std::string> *text) {
  boost::filesystem::path p(name);
  try {
    if (boost::filesystem::is_regular_file(p)) {
      try_adding_file(p, text);
      return true;
    } else if (boost::filesystem::is_directory(p)) {
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
