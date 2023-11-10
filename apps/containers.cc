#include <string>

#include "src/cottontail.h"

int main(int argc, char **argv) {
  std::string error;
  std::shared_ptr<cottontail::Warren> warren;
  std::string burrow = "";
  if (argc > 1)
    burrow = argv[1];
  warren = cottontail::Warren::make(burrow, &error);
  if (warren == nullptr) {
    std::cerr << argv[0] << ": " << error << "\n";
    return 1;
  }
  std::string container_key = "container";
  std::string container_gcl;
  warren->start();
  warren->get_parameter(container_key, &container_gcl);
  if (container_gcl == "") {
    std::cerr << argv[0] << ":  no container query in burrow\n";
    return 1;
  }
  std::unique_ptr<cottontail::Hopper> hopper =
      warren->hopper_from_gcl(container_gcl, &error);
  if (hopper == nullptr) {
    std::cerr << argv[0] << ":  bad container query in burrow\n";
    return 1;
  }
  auto oneline = [](std::string s) {
    for (size_t i = 0; i < s.length(); i++)
      if (s[i] == '\n' || s[i] == '\r')
        s[i] = ' ';
    return s;
  };
  cottontail::addr k = cottontail::minfinity + 1, p, q;
  for (hopper->tau(k, &p, &q); p < cottontail::maxfinity;
       hopper->tau(k, &p, &q)) {
    k = p + 1;
    std::string container = oneline(warren->txt()->translate(p, q));
    std::cout << container << "\n";
  }
  warren->end();
  return 0;
}
