#include <fstream>
#include <memory>
#include <string>

#include "src/cottontail.h"

void usage(std::string program_name) {
  std::cerr << "usage: " << program_name << "[filename]\n";
}

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  std::string error;
  std::string burrow = "wikidata.burrow";
  std::shared_ptr<cottontail::Working> working =
      cottontail::Working::mkdir(burrow, &error);
  if (working == nullptr) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }
  std::string options = "idx:fvalue_compressor:tfdf tokenizer:name:utf8 "
                        "txt:json:yes featurizer@json";
  std::shared_ptr<cottontail::Builder> builder =
      cottontail::SimpleBuilder::make(working, options, &error);
  if (builder == nullptr) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }
  std::shared_ptr<cottontail::Scribe> scribe =
      cottontail::Scribe::make(builder, &error);
  if (scribe == nullptr) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }
  size_t i = 0;
  std::string line;
  cottontail::addr p, q;
  while (std::getline(std::cin, line)) {
    i++;
    if (line != "" && line != "[" && line != "]") {
      if (!json_scribe(line.substr(0, line.length() - 1), scribe, &p, &q, &error)) {
        std::cerr << program_name << ": On input line: " << i << ": " << error
                  << "\n";
        return 1;
      }
    }
    if (i % 1000 == 0)
      std::cerr << i << "\n";
  }
  scribe->finalize();
  return 0;
}
