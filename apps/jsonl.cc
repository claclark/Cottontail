#include <memory>
#include <string>
#include <vector>

#include "src/cottontail.h"

void usage(std::string program_name) {
  std::cerr << "usage: " << program_name << " [files]\n";
}

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  if (argc == 2 && argv[1] == std::string("--help")) {
    usage(program_name);
    return 1;
  }
  std::vector<std::string> filenames;
  for (int i = 1; i < argc; i++)
    filenames.emplace_back(argv[i]);
  std::string error;
  std::string burrow = "json.burrow";
  std::shared_ptr<cottontail::Working> working =
      cottontail::Working::mkdir(burrow, &error);
  if (working == nullptr) {
    std::cerr << program_name + ": " + error + "\n";
    return 1;
  }
  std::string options = "tokenizer:name:utf8 txt:json:yes";
  std::shared_ptr<cottontail::Builder> builder =
      cottontail::SimpleBuilder::make(working, options, &error);
  if (builder == nullptr) {
    std::cerr << program_name + ": " + error + "\n";
    return 1;
  }
  std::shared_ptr<cottontail::Scribe> scribe =
      cottontail::Scribe::make(builder, &error);
  if (scribe == nullptr) {
    std::cerr << program_name + ": " + error + "\n";
    return 1;
  }
  if (!scribe_jsonl(filenames, scribe, &error)){
    std::cerr << program_name + ": " + error + "\n";
    return 1;
  }
  scribe->finalize();
  return 0;
}
