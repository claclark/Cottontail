#include <memory>
#include <string>
#include <vector>

#include "src/cottontail.h"

void usage(std::string program_name) {
  std::cerr << "usage: " << program_name << " [--simple|---bigwig] [files]\n";
}

std::shared_ptr<cottontail::Scribe> simple_scribe(const std::string &burrow,
                                                  std::string *error) {
  std::shared_ptr<cottontail::Working> working =
      cottontail::Working::mkdir(burrow, error);
  if (working == nullptr)
    return nullptr;
  std::string options = "tokenizer:name:utf8 txt:json:yes featurizer@json";
  std::shared_ptr<cottontail::Builder> builder =
      cottontail::SimpleBuilder::make(working, options, error);
  if (builder == nullptr)
    return nullptr;
  return cottontail::Scribe::make(builder, error);
}

std::shared_ptr<cottontail::Scribe> bigwig_scribe(const std::string &burrow,
                                                  std::string *error) {
  std::string options = "tokenizer:name:utf8 txt:json:yes featurizer@json";
  std::shared_ptr<cottontail::Bigwig> bigwig =
      cottontail::Bigwig::make(burrow, options, error);
  if (bigwig == nullptr)
    return nullptr;
  return cottontail::Scribe::make(bigwig, error);
}

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  if (argc == 2 && argv[1] == std::string("--help")) {
    usage(program_name);
    return 0;
  }
  if (argc == 1)
    return 0;
  bool simple = true;
  if (argv[1] == std::string("--simple")) {
    --argc;
    argv++;
  } else if (argv[1] == std::string("--bigwig")) {
    simple = false;
    --argc;
    argv++;
  }
  std::string burrow = "json.burrow";
  std::vector<std::string> filenames;
  for (int i = 1; i < argc; i++)
    filenames.emplace_back(argv[i]);
  std::string error;
  std::shared_ptr<cottontail::Scribe> scribe;
  if (simple)
    scribe = simple_scribe(burrow, &error);
  else
    scribe = bigwig_scribe(burrow, &error);
  if (scribe == nullptr) {
    std::cerr << program_name + ": " + error + "\n";
    return 1;
  }
  if (!scribe_jsonl(filenames, scribe, &error)) {
    std::cerr << program_name + ": " + error + "\n";
    return 1;
  }
  scribe->finalize();
  return 0;
}
