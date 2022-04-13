#include <iostream>
#include <string>

#include "src/cottontail.h"

void usage(std::string program_name) {
  std::cerr << "usage: " << program_name
            << " [--burrow burrow] "
               "stemmer_name container_query id_query";
}

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  if (argc == 2 && argv[1] == std::string("--help")) {
    usage(program_name);
    return 0;
  }
  std::string burrow = cottontail::DEFAULT_BURROW;
  std::string stemmer_name;
  std::string container_query;
  std::string id_query;
  if (argc > 2 &&
      (argv[1] == std::string("-b") || argv[1] == std::string("--burrow"))) {
    burrow = argv[2];
    argc -= 2;
    argv += 2;
  }
  if (argc == 4) {
    stemmer_name = argv[1];
    container_query = argv[2];
    id_query = argv[3];
  } else {
    usage(program_name);
    return 1;
  }
  std::string stemmer_recipe = "";
  std::string error;
  std::shared_ptr<cottontail::Stemmer> stemmer =
      cottontail::Stemmer::make(stemmer_name, stemmer_recipe, &error);
  if (stemmer == nullptr) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }
  std::string simple = "simple";
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::Warren::make(simple, burrow, &error);
  if (warren == nullptr) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }
  warren->start();
  if (!warren->set_stemmer(stemmer, &error)) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }
  std::unique_ptr<cottontail::Hopper> container_hopper =
      warren->hopper_from_gcl(container_query, &error);
  if (container_hopper == nullptr) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }
  if (!warren->set_default_container(container_query, &error)) {
    std::cerr << program_name << ": " << error << "\n";
    return -1;
  }
  std::unique_ptr<cottontail::Hopper> id_hopper =
      warren->hopper_from_gcl(id_query, &error);
  if (id_hopper == nullptr) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }
  std::string id_key = "id";
  if (!warren->set_parameter(id_key, id_query, &error)) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }
  if (!cottontail::tf_df_annotations(warren, &error)) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }
  warren->end();
  return 0;
}
