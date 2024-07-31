#include "src/core.h"

#include <chrono>
#include <ctime>
#include <iostream>
#include <regex>
#include <string>

namespace cottontail {
bool okay(const std::string &value) {
  return value == "y" || value == "yes" || value == "on" || value == "true" ||
         value == "okay" || value == "ok" || value == "Y" || value == "Yes" ||
         value == "On" || value == "True" || value == "Okay" || value == "Ok" ||
         value == "OK";
}

std::string okay(bool yes) {
  if (yes)
    return "yes";
  else
    return "no";
}

void stamp(std::string label) {
  std::time_t now = std::time(nullptr);
  if (label != "")
    std::cerr << label << ": " << now << "\n" << std::flush;
  else
    std::cerr << now << "\n" << std::flush;
}

addr now() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

std::vector<std::string> split(std::string str, std::string pattern) {
  while (str.size() > 0 && (str.back() == '\n' || str.back() == '\r'))
    str.pop_back();
  std::regex r(pattern);
  std::vector<std::string> fields{
      std::sregex_token_iterator(str.begin(), str.end(), r, -1), {}};
  return fields;
}

} // namespace cottontail
