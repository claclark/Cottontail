#include <iostream>
#include <string>
#include <string_view>

#include "src/monitor.h"

int main(int argc, char **argv) {
  const bool help = argc == 2 && std::string_view(argv[1]) == "--help";
  if (argc != 2 || help) {
    std::cerr << "usage: " << argv[0] << " directory\n";
    return help ? 0 : 1;
  }

  std::string error;
  auto monitor = cottontail::Monitor::make(argv[1], &error);
  if (monitor == nullptr) {
    std::cerr << argv[0] << ": " << error << "\n";
    return 1;
  }
  for (std::string path = monitor->next(); !path.empty(); path = monitor->next()) {
    std::cout << path << std::endl;
    if (!std::cout)
      return 1;
  }
  return 0;
}
