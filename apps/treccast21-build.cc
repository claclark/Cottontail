#include <iostream>

#include "src/cottontail.h"

bool add_collection(const std::string &location,
                    std::shared_ptr<cottontail::Builder> builder,
                    std::string *error) {
  std::ifstream f(location, std::istream::in);
  if (f.fail()) {
    cottontail::safe_error(error) = "Cannot open: " + location;
    return false;
  }
  std::string line;
  while (std::getline(f, line)) {
    std::string id = line.substr(0, line.find("\t"));
    std::string content = line.substr(line.find("\t"));
    cottontail::addr p_pid, q_pid;
    if (!builder->add_text(id, &p_pid, &q_pid, error))
      return false;
    if (!builder->add_annotation(":pid", p_pid, q_pid, 0.0, error))
      return false;
    cottontail::addr p_content, q_content;
    if (!builder->add_text(content, &p_content, &q_content, error))
      return false;
    if (!builder->add_annotation(":paragraph", p_pid, q_content, 0.0, error))
      return false;
  }
  return true;
}

void usage(std::string program_name) {
  std::cerr << "usage: " << program_name << " [--burrow burrow] tsv-file...\n";
}

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  std::string burrow = cottontail::DEFAULT_BURROW;
  if (argc == 2 && argv[1] == std::string("--help")) {
    usage(program_name);
    return 0;
  }
  if (argc > 2 &&
      (argv[1] == std::string("-b") || argv[1] == std::string("--burrow"))) {
    burrow = argv[2];
    argc -= 2;
    argv += 2;
  }
  if (argc == 1) {
    usage(program_name);
    return 1;
  }
  std::string error;
  std::shared_ptr<cottontail::Working> working =
      cottontail::Working::mkdir(burrow, &error);
  if (working == nullptr) {
    std::cerr << program_name << ": " << error << "\n";
    return -1;
  }
  std::string options = "tokenizer:noxml";
  std::shared_ptr<cottontail::Builder> builder =
      cottontail::SimpleBuilder::make(working, options, &error);
  if (builder == nullptr) {
    std::cerr << program_name << ": " << error << "\n";
    return -1;
  }
  for (int i = 1; i < argc; i++) {
    std::string location = argv[i];
    if (!add_collection(location, builder, &error)) {
      std::cerr << program_name << ": " << error << ": building: " << location
                << "\n";
      return -1;
    }
  }
  if (!builder->finalize(&error)) {
    std::cerr << program_name << ": " << error << "\n";
    return -1;
  }
  return 0;
}
