#include <iostream>

#include "src/cottontail.h"
#include "src/json.h"
#include "src/nlohmann.h"

void usage(std::string program_name) {
  std::cerr << "usage: " << program_name << " [--burrow burrow] vectors\n";
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
  if (argc != 2) {
    usage(program_name);
    return 1;
  }
  std::string vectors = argv[1];
  std::string error;
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::Warren::make("simple", burrow, &error);
  if (warren == nullptr) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }
  warren->start();
  std::string content_key = "container";
  std::string content_query = "";
  if (!warren->get_parameter(content_key, &content_query, &error))
    content_query = warren->default_container();
  if (content_query == "") {
    std::cerr << program_name << ": can't find a definition for item content\n";
    warren->end();
    return 1;
  }
  std::string id_query = "";
  if (!warren->get_parameter("id", &id_query, &error)) {
    std::cerr << program_name
              << ": can't find a definition for item identity\n";
    warren->end();
    return 1;
  }
  std::shared_ptr<cottontail::Featurizer> featurizer =
      cottontail::TaggingFeaturizer::make(warren->featurizer(), "splade",
                                          &error);
  if (featurizer == nullptr) {
    std::cerr << program_name << ": error: " << error << "\n";
    warren->end();
    return 1;
  }
  std::ifstream vectorsf(vectors);
  if (vectorsf.fail()) {
    std::cerr << program_name << ": can't open splade vectors: " << vectors
              << "\n";
    warren->end();
    return 1;
  }
  if (!warren->annotator()->transaction(&error)) {
    std::cerr << program_name << ": error: " << error << "\n";
    warren->end();
    return 1;
  }
  std::string line;
  unsigned number = 0;
  while (std::getline(vectorsf, line)) {
    number++;
    json j;
    try {
      j = json::parse(line);
    } catch (json::parse_error &e) {
      std::cerr << program_name << ": can't parse JSON: " << number << ": "
                << vectors << "\n";
      continue;
    }
    if (!j["splade_vector"].is_object()) {
      std::cerr << program_name << ": record missing splade vector: " << number
                << ": " << vectors << "\n";
      continue;
    }
    cottontail::addr p, q;
    {
      if (!j["docid"].is_string()) {
        std::cerr << program_name << ": record missing docid: " << number
                  << ": " << vectors << "\n";
        continue;
      }
      std::string docid = j["docid"];
      std::string query =
          "(>> " + content_query + " (>> " + id_query + " \"" + docid + "\" ))";
      std::unique_ptr<cottontail::Hopper> hopper =
          warren->hopper_from_gcl(query, &error);
      if (hopper == nullptr) {
        std::cerr << program_name << ": error: " << error << "\n";
        warren->annotator()->abort();
        warren->end();
        return 1;
      }
      hopper->tau(0, &p, &q);
      if (p == cottontail::maxfinity) {
        std::cerr << program_name << ": docid not in collection: " << docid
                  << "\n";
        continue;
      }
    }
    for (auto &element : j["splade_vector"].items()) {
      cottontail::addr value = 0;
      if (element.value().is_number_integer())
        value = element.value();
      if (value > 0) {
        cottontail::addr feature = featurizer->featurize(element.key());
        if (!warren->annotator()->annotate(feature, p, p, value, &error)) {
          std::cerr << program_name << ": error: " << error << "\n";
          warren->annotator()->abort();
          warren->end();
          return 1;
        }
      }
    }
  }
  if (warren->annotator()->ready()) {
    warren->annotator()->commit();
  } else {
    std::cerr << program_name << ": commit failed\n";
    warren->annotator()->abort();
  }
  warren->end();
  return 0;
}
