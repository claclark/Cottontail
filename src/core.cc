#include "src/core.h"

#include <chrono>
#include <ctime>
#include <iostream>
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

} // namespace cottontail
