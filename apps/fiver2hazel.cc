#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>

#include "src/compressor.h"
#include "src/dna.h"
#include "src/featurizer.h"
#include "src/fiver.h"
#include "src/recipe.h"
#include "src/tokenizer.h"
#include "src/working.h"

namespace {

void usage(const std::string &program_name) {
  std::cerr << "usage: " << program_name << " [--chunk-size bytes] fiver...\n";
}

std::string dirname(const std::string &path) {
  size_t slash = path.find_last_of('/');
  if (slash == std::string::npos)
    return ".";
  if (slash == 0)
    return "/";
  return path.substr(0, slash);
}

std::string basename(const std::string &path) {
  size_t slash = path.find_last_of('/');
  if (slash == std::string::npos)
    return path;
  return path.substr(slash + 1);
}

bool package(const std::string &dna, const std::string &key,
             std::string *name, std::string *recipe, std::string *error) {
  if (!cottontail::name_and_recipe(dna, key, error, name, recipe))
    return false;
  return true;
}

bool compressor_from_recipe(const std::string &recipe,
                            const std::string &name_key,
                            const std::string &recipe_key,
                            std::shared_ptr<cottontail::Compressor> *compressor,
                            std::string *error) {
  std::map<std::string, std::string> parameters;
  if (!cottontail::cook(recipe, &parameters, error))
    return false;
  auto name = parameters.find(name_key);
  auto subrecipe = parameters.find(recipe_key);
  if (name == parameters.end() || subrecipe == parameters.end()) {
    cottontail::safe_error(error) =
        "Missing compressor setting in recipe: " + recipe;
    return false;
  }
  *compressor =
      cottontail::Compressor::make(name->second, subrecipe->second, error);
  return *compressor != nullptr;
}

} // namespace

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  if (argc == 2 && argv[1] == std::string("--help")) {
    usage(program_name);
    return 0;
  }
  if (argc < 2) {
    usage(program_name);
    return 1;
  }

  cottontail::addr text_chunk_size = 64 * 1024;
  int first_fiver = 1;
  if (argc >= 4 && argv[1] == std::string("--chunk-size")) {
    try {
      text_chunk_size = std::stoll(argv[2]);
    } catch (const std::invalid_argument &e) {
      usage(program_name);
      return 1;
    } catch (const std::out_of_range &e) {
      usage(program_name);
      return 1;
    }
    if (text_chunk_size <= 0) {
      usage(program_name);
      return 1;
    }
    first_fiver = 3;
  }
  if (first_fiver >= argc) {
    usage(program_name);
    return 1;
  }

  int status = 0;
  for (int i = first_fiver; i < argc; i++) {
    std::string fiver_path = argv[i];
    std::string dir = dirname(fiver_path);
    std::string file = basename(fiver_path);
    std::string error;
    std::shared_ptr<cottontail::Working> working =
        cottontail::Working::make(dir, &error);
    if (working == nullptr) {
      std::cerr << program_name << ": " << error << "\n";
      status = 1;
      continue;
    }

    std::string dna;
    if (!cottontail::read_dna(working, &dna, &error)) {
      std::cerr << program_name << ": " << error << "\n";
      status = 1;
      continue;
    }
    std::map<std::string, std::string> source_parameters;
    if (!cottontail::cook(dna, &source_parameters, &error)) {
      std::cerr << program_name << ": " << error << "\n";
      status = 1;
      continue;
    }
    std::string warren_parameters;
    auto parameters = source_parameters.find("parameters");
    if (parameters != source_parameters.end())
      warren_parameters = parameters->second;

    std::string featurizer_name, featurizer_recipe;
    std::string tokenizer_name, tokenizer_recipe;
    std::string idx_name, idx_recipe;
    std::string txt_name, txt_recipe;
    if (!package(dna, "featurizer", &featurizer_name, &featurizer_recipe,
                 &error) ||
        !package(dna, "tokenizer", &tokenizer_name, &tokenizer_recipe,
                 &error) ||
        !package(dna, "idx", &idx_name, &idx_recipe, &error) ||
        !package(dna, "txt", &txt_name, &txt_recipe, &error)) {
      std::cerr << program_name << ": " << error << "\n";
      status = 1;
      continue;
    }

    std::shared_ptr<cottontail::Featurizer> featurizer =
        cottontail::Featurizer::make(featurizer_name, featurizer_recipe,
                                     &error, working);
    if (featurizer == nullptr) {
      std::cerr << program_name << ": " << error << "\n";
      status = 1;
      continue;
    }
    std::shared_ptr<cottontail::Tokenizer> tokenizer =
        cottontail::Tokenizer::make(tokenizer_name, tokenizer_recipe, &error);
    if (tokenizer == nullptr) {
      std::cerr << program_name << ": " << error << "\n";
      status = 1;
      continue;
    }

    std::shared_ptr<cottontail::Compressor> posting_compressor;
    std::shared_ptr<cottontail::Compressor> fvalue_compressor;
    std::shared_ptr<cottontail::Compressor> text_compressor;
    if (!compressor_from_recipe(idx_recipe, "posting_compressor",
                                "posting_compressor_recipe",
                                &posting_compressor, &error) ||
        !compressor_from_recipe(idx_recipe, "fvalue_compressor",
                                "fvalue_compressor_recipe",
                                &fvalue_compressor, &error) ||
        !compressor_from_recipe(txt_recipe, "compressor", "compressor_recipe",
                                &text_compressor, &error)) {
      std::cerr << program_name << ": " << error << "\n";
      status = 1;
      continue;
    }

    std::shared_ptr<cottontail::Fiver> fiver = cottontail::Fiver::unpickle(
        file, working, featurizer, tokenizer, &error, posting_compressor,
        fvalue_compressor, text_compressor);
    if (fiver == nullptr) {
      std::cerr << program_name << ": " << error << "\n";
      status = 1;
      continue;
    }
    fiver->start();
    if (!fiver->hazel(&error, false, text_chunk_size, warren_parameters)) {
      std::cerr << program_name << ": " << error << "\n";
      fiver->end();
      status = 1;
      continue;
    }
    fiver->end();
  }
  return status;
}
