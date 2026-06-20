#include <algorithm>
#include <cstdio>
#include <exception>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "src/compressor.h"
#include "src/core.h"
#include "src/dna.h"
#include "src/featurizer.h"
#include "src/fiver.h"
#include "src/hazel.h"
#include "src/recipe.h"
#include "src/tokenizer.h"
#include "src/working.h"

namespace {

struct ShardName {
  cottontail::addr start = 0;
  cottontail::addr end = 0;
  std::string name;
};

void usage(const std::string &program_name) {
  std::cerr << "usage: " << program_name
            << " [--chunk-size bytes] [--convert] [--merge] burrow\n";
}

bool parse_shard_name(const std::string &name, const std::string &prefix,
                      cottontail::addr *start, cottontail::addr *end) {
  std::string full_prefix = prefix + ".";
  if (name.compare(0, full_prefix.size(), full_prefix) != 0)
    return false;
  size_t dot = name.find('.', full_prefix.size());
  if (dot == std::string::npos)
    return false;
  std::string start_string =
      name.substr(full_prefix.size(), dot - full_prefix.size());
  std::string end_string = name.substr(dot + 1);
  if (start_string.empty() || end_string.empty())
    return false;
  for (char c : start_string)
    if (c < '0' || c > '9')
      return false;
  for (char c : end_string)
    if (c < '0' || c > '9')
      return false;
  try {
    *start = std::stoll(start_string);
    *end = std::stoll(end_string);
  } catch (const std::exception &) {
    return false;
  }
  return *start >= 0 && *end >= *start;
}

std::string shard_name(const std::string &prefix, cottontail::addr start,
                       cottontail::addr end) {
  auto seq2str = [](cottontail::addr sequence) {
    std::stringstream ss;
    ss.fill('0');
    ss.width(20);
    ss << sequence;
    return ss.str();
  };
  return prefix + "." + seq2str(start) + "." + seq2str(end);
}

bool living_fivers(std::shared_ptr<cottontail::Working> working,
                   std::vector<ShardName> *living, std::string *error) {
  std::vector<std::string> names = working->ls("fiver");
  std::vector<ShardName> found;
  for (auto &name : names) {
    ShardName shard;
    shard.name = name;
    if (!parse_shard_name(name, "fiver", &shard.start, &shard.end))
      continue;
    found.push_back(shard);
  }
  std::sort(found.begin(), found.end(), [](const ShardName &a,
                                           const ShardName &b) {
    return a.start < b.start || (a.start == b.start && a.end > b.end);
  });

  living->clear();
  for (auto &shard : found) {
    if (living->empty() || living->back().end < shard.start) {
      living->push_back(shard);
    } else if (living->back().end >= shard.end) {
      continue;
    } else {
      cottontail::safe_error(error) =
          "Overlapping fiver shard ranges around: " + shard.name;
      return false;
    }
  }
  return true;
}

bool hazel_files(std::shared_ptr<cottontail::Working> working,
                 std::vector<ShardName> *hazels, std::string *error) {
  hazels->clear();
  for (auto &name : working->ls("hazel")) {
    ShardName shard;
    shard.name = name;
    if (!parse_shard_name(name, "hazel", &shard.start, &shard.end))
      continue;
    hazels->push_back(shard);
  }
  std::sort(hazels->begin(), hazels->end(), [](const ShardName &a,
                                               const ShardName &b) {
    return a.start < b.start || (a.start == b.start && a.end < b.end);
  });
  return true;
}

bool remove_hazels(std::shared_ptr<cottontail::Working> working,
                   const std::vector<ShardName> &hazels, std::string *error) {
  for (auto &hazel : hazels) {
    if (std::remove(working->make_name(hazel.name).c_str()) != 0) {
      cottontail::safe_error(error) =
          "Could not remove Hazel shard: " + hazel.name;
      return false;
    }
  }
  return true;
}

bool covered_by_other_hazels(const std::vector<ShardName> &hazels,
                             size_t candidate) {
  cottontail::addr next = hazels[candidate].start;
  size_t pieces = 0;
  while (next <= hazels[candidate].end) {
    bool found = false;
    cottontail::addr best_end = -1;
    for (size_t i = 0; i < hazels.size(); i++) {
      if (i == candidate || hazels[i].start != next ||
          hazels[i].end > hazels[candidate].end)
        continue;
      if (!found || hazels[i].end > best_end) {
        found = true;
        best_end = hazels[i].end;
      }
    }
    if (!found)
      return false;
    pieces++;
    if (best_end == hazels[candidate].end)
      return pieces >= 2;
    next = best_end + 1;
  }
  return false;
}

bool remove_merged_hazels(std::shared_ptr<cottontail::Working> working,
                          std::vector<ShardName> *hazels, size_t *removed,
                          std::string *error) {
  std::vector<bool> discard(hazels->size(), false);
  for (size_t i = 0; i < hazels->size(); i++)
    discard[i] = covered_by_other_hazels(*hazels, i);

  std::vector<ShardName> survivors;
  std::vector<ShardName> doomed;
  for (size_t i = 0; i < hazels->size(); i++) {
    if (discard[i])
      doomed.push_back((*hazels)[i]);
    else
      survivors.push_back((*hazels)[i]);
  }
  if (!remove_hazels(working, doomed, error))
    return false;
  *hazels = survivors;
  if (removed != nullptr)
    *removed = doomed.size();
  return true;
}

bool contiguous_hazels(const std::vector<ShardName> &hazels,
                       std::string *error) {
  for (size_t i = 1; i < hazels.size(); i++) {
    if (hazels[i - 1].end + 1 != hazels[i].start) {
      cottontail::safe_error(error) =
          "Hazel merge got non-contiguous shards around: " + hazels[i].name;
      return false;
    }
  }
  return true;
}

bool package(const std::string &dna, const std::string &key,
             std::string *name, std::string *recipe, std::string *error) {
  return cottontail::name_and_recipe(dna, key, error, name, recipe);
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

bool make_components(
    std::shared_ptr<cottontail::Working> working,
    std::shared_ptr<cottontail::Featurizer> *featurizer,
    std::shared_ptr<cottontail::Tokenizer> *tokenizer,
    std::shared_ptr<cottontail::Compressor> *posting_compressor,
    std::shared_ptr<cottontail::Compressor> *fvalue_compressor,
    std::shared_ptr<cottontail::Compressor> *text_compressor,
    std::string *warren_parameters, std::string *error) {
  std::string dna;
  if (!cottontail::read_dna(working, &dna, error))
    return false;

  std::map<std::string, std::string> source_parameters;
  if (!cottontail::cook(dna, &source_parameters, error))
    return false;
  auto parameters = source_parameters.find("parameters");
  if (parameters == source_parameters.end())
    *warren_parameters = "";
  else
    *warren_parameters = parameters->second;

  std::string featurizer_name, featurizer_recipe;
  std::string tokenizer_name, tokenizer_recipe;
  std::string idx_name, idx_recipe;
  std::string txt_name, txt_recipe;
  if (!package(dna, "featurizer", &featurizer_name, &featurizer_recipe,
               error) ||
      !package(dna, "tokenizer", &tokenizer_name, &tokenizer_recipe, error) ||
      !package(dna, "idx", &idx_name, &idx_recipe, error) ||
      !package(dna, "txt", &txt_name, &txt_recipe, error))
    return false;

  *featurizer =
      cottontail::Featurizer::make(featurizer_name, featurizer_recipe, error,
                                   working);
  if (*featurizer == nullptr)
    return false;
  *tokenizer =
      cottontail::Tokenizer::make(tokenizer_name, tokenizer_recipe, error);
  if (*tokenizer == nullptr)
    return false;

  return compressor_from_recipe(idx_recipe, "posting_compressor",
                                "posting_compressor_recipe",
                                posting_compressor, error) &&
         compressor_from_recipe(idx_recipe, "fvalue_compressor",
                                "fvalue_compressor_recipe", fvalue_compressor,
                                error) &&
         compressor_from_recipe(txt_recipe, "compressor", "compressor_recipe",
                                text_compressor, error);
}

} // namespace

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  if (argc == 2 && argv[1] == std::string("--help")) {
    usage(program_name);
    return 0;
  }

  cottontail::addr text_chunk_size = 64 * 1024;
  bool convert = false;
  bool merge = false;
  bool got_mode = false;
  int arg = 1;
  while (arg < argc) {
    std::string option = argv[arg];
    if (option == "--chunk-size" && arg + 1 < argc) {
      try {
        text_chunk_size = std::stoll(argv[arg + 1]);
      } catch (const std::exception &) {
        usage(program_name);
        return 1;
      }
      if (text_chunk_size <= 0) {
        usage(program_name);
        return 1;
      }
      arg += 2;
    } else if (option == "--convert") {
      convert = true;
      got_mode = true;
      arg++;
    } else if (option == "--merge") {
      merge = true;
      got_mode = true;
      arg++;
    } else {
      break;
    }
  }

  if (arg + 1 != argc) {
    usage(program_name);
    return 1;
  }
  if (!got_mode) {
    convert = true;
    merge = true;
  }
  std::string burrow = argv[arg];

  std::string error;
  std::shared_ptr<cottontail::Working> working =
      cottontail::Working::make(burrow, &error);
  if (working == nullptr) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }

  std::shared_ptr<cottontail::Featurizer> featurizer;
  std::shared_ptr<cottontail::Tokenizer> tokenizer;
  std::shared_ptr<cottontail::Compressor> posting_compressor;
  std::shared_ptr<cottontail::Compressor> fvalue_compressor;
  std::shared_ptr<cottontail::Compressor> text_compressor;
  std::string warren_parameters;
  if (!make_components(working, &featurizer, &tokenizer, &posting_compressor,
                       &fvalue_compressor, &text_compressor,
                       &warren_parameters, &error)) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }

  cottontail::addr total_start = cottontail::now();
  cottontail::addr convert_milliseconds = 0;
  if (convert) {
    std::vector<ShardName> hazels;
    if (!hazel_files(working, &hazels, &error)) {
      std::cerr << program_name << ": " << error << "\n";
      return 1;
    }

    std::vector<ShardName> fivers;
    if (!living_fivers(working, &fivers, &error)) {
      std::cerr << program_name << ": " << error << "\n";
      return 1;
    }
    if (fivers.empty()) {
      std::cerr << program_name << ": no fiver shards found in: " << burrow
                << "\n";
      return 1;
    }

    for (auto &fiver_name : fivers) {
      std::string hazel_name =
          shard_name("hazel", fiver_name.start, fiver_name.end);
      if (std::binary_search(
              hazels.begin(), hazels.end(),
              ShardName{fiver_name.start, fiver_name.end, hazel_name},
              [](const ShardName &a, const ShardName &b) {
                return a.start < b.start ||
                       (a.start == b.start && a.end < b.end);
              })) {
        std::cerr << program_name << ": keeping existing " << hazel_name
                  << "\n";
        continue;
      }
      std::shared_ptr<cottontail::Fiver> fiver = cottontail::Fiver::unpickle(
          fiver_name.name, working, featurizer, tokenizer, &error,
          posting_compressor, fvalue_compressor, text_compressor);
      if (fiver == nullptr) {
        std::cerr << program_name << ": " << error << "\n";
        return 1;
      }
      fiver->start();
      cottontail::addr t0 = cottontail::now();
      if (fiver->hazel(&error, text_chunk_size, warren_parameters) ==
          nullptr) {
        std::cerr << program_name << ": " << error << "\n";
        fiver->end();
        return 1;
      }
      cottontail::addr t1 = cottontail::now();
      fiver->end();
      convert_milliseconds += t1 - t0;
      std::cerr << program_name << ": converted " << hazel_name << " in "
                << (t1 - t0) << " millisecond(s)\n";
    }
    std::cerr << program_name << ": Conversion took: " << convert_milliseconds
              << " millisecond(s)\n";
  }

  cottontail::addr merge_milliseconds = 0;
  if (merge) {
    std::vector<ShardName> hazels;
    size_t removed = 0;
    if (!hazel_files(working, &hazels, &error) ||
        !remove_merged_hazels(working, &hazels, &removed, &error) ||
        !contiguous_hazels(hazels, &error)) {
      std::cerr << program_name << ": " << error << "\n";
      return 1;
    }
    if (removed > 0)
      std::cerr << program_name << ": removed " << removed
                << " merged Hazel shard(s)\n";
    if (hazels.empty()) {
      std::cerr << program_name << ": no Hazel shards found in: " << burrow
                << "\n";
      return 1;
    } else if (hazels.size() == 1) {
      std::cerr << program_name << ": one Hazel shard available; nothing to "
                << "merge\n";
    } else {
      std::vector<std::string> names;
      for (auto &hazel : hazels)
        names.push_back(hazel.name);
      cottontail::addr t0 = cottontail::now();
      if (!cottontail::Hazel::merge(working, names, warren_parameters, &error)) {
        std::cerr << program_name << ": " << error << "\n";
        return 1;
      }
      cottontail::addr t1 = cottontail::now();
      merge_milliseconds = t1 - t0;
      std::string final_hazel =
          shard_name("hazel", hazels.front().start, hazels.back().end);
      std::cerr << program_name << ": Merge took: " << merge_milliseconds
                << " millisecond(s)\n";
      std::cerr << program_name << ": final Hazel is " << burrow << "/"
                << final_hazel << "\n";
    }
  }

  cottontail::addr total_end = cottontail::now();
  std::cerr << program_name << ": Total took: " << (total_end - total_start)
            << " millisecond(s)\n";
  return 0;
}
