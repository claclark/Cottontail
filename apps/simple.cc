#include <exception>
#include <iostream>
#include <memory>
#include <regex>
#include <string>
#include <vector>

#include "apps/collection.h"
#include "apps/walk.h"
#include "src/cottontail.h"

namespace {

bool trec_build(const std::vector<std::string> &text, const std::string options,
                std::shared_ptr<cottontail::Working> working,
                std::string *error = nullptr, bool verbose = true) {
  std::shared_ptr<cottontail::Builder> builder =
      cottontail::SimpleBuilder::make(working, options, error);
  if (builder == nullptr)
    return false;
  builder->verbose(verbose);
  return cottontail::build_trec(text, builder, error);
}

bool marco_build(const std::vector<std::string> &text, std::string options,
                 std::shared_ptr<cottontail::Working> working,
                 std::string *error = nullptr, bool verbose = true) {
  if (text.size() == 0)
    return true;
  if (text.size() > 1) {
    cottontail::safe_set(error) = "multiple files for MS MARCO build";
    return false;
  }
  options += " tokenizer:noxml";
  std::shared_ptr<cottontail::Builder> builder =
      cottontail::SimpleBuilder::make(working, options, error);
  if (builder == nullptr)
    return false;
  builder->verbose(verbose);
  if (!collection_TREC_MARCO(text[0], builder, error))
    return false;
  if (!builder->finalize(error))
    return false;
  return true;
}

bool marco2_build(const std::vector<std::string> &text, std::string options,
                  std::shared_ptr<cottontail::Working> working,
                  std::string *error = nullptr, bool verbose = true) {
  if (text.size() == 0)
    return true;
  options += " tokenizer:noxml";
  std::shared_ptr<cottontail::Builder> builder =
      cottontail::SimpleBuilder::make(working, options, error);
  if (builder == nullptr)
    return false;
  builder->verbose(verbose);
  for (auto &filename : text) {
    if (!collection_MSMARCO_V2(filename, builder, error))
      return false;
  }
  if (!builder->finalize(error))
    return false;
  return true;
}

bool json_build(const std::vector<std::string> &text,
                std::shared_ptr<cottontail::Working> working,
                std::string *error, bool verbose = false) {
  return false;
}
} // namespace

int main(int argc, char *argv[]) {
  std::string usage = "usage: ";
  std::string program_name = argv[0];
  usage += program_name;
  usage += " [options] [--trec|--json|--marco|--marco2] --files FILES...\n";

  std::string options = "";
  for (argv++, --argc; argc > 0 && argv[0][0] != '-'; argv++, --argc)
    options += " " + std::string(*argv);

  if (argc < 2) {
    std::cerr << usage;
    return -1;
  }

  bool verbose = true;
  std::string builder = "trec";
  std::string argv0 = argv[0];
  if (argv0 != "--files") {
    if (argv0 == "--json") {
      builder = "json";
    } else if (argv0 == "--marco") {
      builder = "marco";
    } else if (argv0 == "--marco2") {
      builder = "marco2";
    } else if (argv0 != "--trec") {
      std::cerr << usage;
      return -1;
    }
    --argc;
    argv++;
    argv0 = argv[0];
  }

  if (argv0 != "--files" || argc < 2) {
    std::cerr << usage;
    return -1;
  }

  std::vector<std::string> text;
  for (int i = 1; i < argc; i++)
    if (!cottontail::walk_filesystem(argv[i], &text)) {
      std::cerr << program_name << ": can't build \"" << argv[i] << "\"\n";
      return -1;
    }

  std::string error;
  std::string burrow = cottontail::DEFAULT_BURROW;
  std::shared_ptr<cottontail::Working> working =
      cottontail::Working::mkdir(burrow, &error);
  if (working == nullptr) {
    std::cerr << program_name << ": " << error << "\n";
    return -1;
  }
  if (builder == "json") {
    if (!json_build(text, working, &error, verbose)) {
      std::cerr << program_name << ": " << error << "\n";
      return -1;
    }
  } else if (builder == "marco") {
    if (!marco_build(text, options, working, &error, verbose)) {
      std::cerr << program_name << ": " << error << "\n";
      return -1;
    }
  } else if (builder == "marco2") {
    if (!marco2_build(text, options, working, &error, verbose)) {
      std::cerr << program_name << ": " << error << "\n";
      return -1;
    }
  } else {
    if (!trec_build(text, options, working, &error, verbose)) {
      std::cerr << program_name << ": " << error << "\n";
      return -1;
    }
  }
#if 0
// TODO: delete this code
  std::string simple = "simple";
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::Warren::make(simple, burrow, &error);
  if (warren == nullptr) {
    std::cerr << program_name << ": " << error << "\n";
    return -1;
  }
  warren->start();
  std::string stemmer_name = "porter";
  std::string stemmer_recipe = "";
  std::shared_ptr<cottontail::Stemmer> stemmer =
      cottontail::Stemmer::make(stemmer_name, stemmer_recipe, &error);
  if (stemmer == nullptr)
    std::cerr << program_name << ": " << error << "\n";
  if (!warren->set_stemmer(stemmer, &error)) {
    std::cerr << program_name << ": " << error << "\n";
    return -1;
  }
  std::string container_query = "(... <DOC> </DOC>)";
  if (!warren->set_default_container(container_query, &error)) {
    std::cerr << program_name << ": " << error << "\n";
    return -1;
  }
  if (verbose)
    std::cout << "Adding tf-idf annontations\n";
  if (!tf_idf_annotations(warren, &error)) {
    std::cerr << program_name << ": " << error << "\n";
    return -1;
  }
  warren->end();
#endif
  if (verbose)
    std::cout << "Done.\n";
  return 0;
}
