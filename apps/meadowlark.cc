#include <cctype>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "apps/walk.h"
#include "meadowlark/meadowlark.h"

void usage(std::string program_name) {
  std::cerr << "usage: " << program_name
            << " [--meadow meadow] [--create [parameter:value ...]]"
            << " [--tsv path...] [--jsonl|--json path...]"
            << " [--text path...] [--code path...]...\n";
}

bool parameter_assignment(const std::string &argument, std::string *key,
                          std::string *value) {
  size_t colon = argument.find(':');
  if (colon == std::string::npos || colon == 0 || colon + 1 == argument.size())
    return false;
  if (!std::isalpha(static_cast<unsigned char>(argument[0])))
    return false;
  for (size_t i = 1; i < colon; i++) {
    unsigned char c = static_cast<unsigned char>(argument[i]);
    if (!std::isalnum(c) && c != '_' && c != '-')
      return false;
  }
  *key = argument.substr(0, colon);
  *value = argument.substr(colon + 1);
  return true;
}

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  std::string error;
  if (argc == 2 && argv[1] == std::string("--help")) {
    usage(program_name);
    return 0;
  }
  if (argc <= 1)
    return 0;
  std::string meadow;
  if (argc > 2 &&
      (argv[1] == std::string("-m") || argv[1] == std::string("--meadow"))) {
    meadow = argv[2];
    argc -= 2;
    argv += 2;
  }
  if (argc <= 1)
    return 0;
  bool create =
      argv[1] == std::string("-c") || argv[1] == std::string("--create");
  std::vector<std::pair<std::string, std::string>> parameters;
  if (create) {
    --argc;
    argv++;
    while (argc > 1 && argv[1][0] != '-') {
      std::string key;
      std::string value;
      if (!parameter_assignment(argv[1], &key, &value)) {
        std::cerr << program_name << ": Invalid parameter " << argv[1]
                  << "\n";
        usage(program_name);
        return 1;
      }
      parameters.emplace_back(key, value);
      --argc;
      argv++;
    }
  }
  std::shared_ptr<cottontail::Warren> warren;
  if (create) {
    if (meadow == "")
      warren = cottontail::meadowlark::create_meadow(&error);
    else
      warren = cottontail::meadowlark::create_meadow(meadow, &error);
  } else {
    if (meadow == "")
      warren = cottontail::meadowlark::open_meadow(&error);
    else
      warren = cottontail::meadowlark::open_meadow(meadow, &error);
  }
  if (warren == nullptr) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }
  for (const auto &parameter : parameters)
    if (!warren->set_parameter(parameter.first, parameter.second, &error)) {
      std::cerr << program_name << ": " << error << "\n";
      return 1;
    }
  cottontail::meadowlark::InputType input_type =
      cottontail::meadowlark::InputType::NONE;
  bool expecting_file = false;
  std::vector<cottontail::meadowlark::InputFile> inputs;
  while (argc > 1) {
    std::string argument = argv[1];
    if (argument == "--tsv") {
      if (expecting_file) {
        std::cerr << program_name << ": Missing file before " << argument
                  << "\n";
        usage(program_name);
        return 1;
      }
      input_type = cottontail::meadowlark::InputType::TSV;
      expecting_file = true;
    } else if (argument == "--jsonl" || argument == "--json") {
      if (expecting_file) {
        std::cerr << program_name << ": Missing file before " << argument
                  << "\n";
        usage(program_name);
        return 1;
      }
      input_type = cottontail::meadowlark::InputType::JSONL;
      expecting_file = true;
    } else if (argument == "--text") {
      if (expecting_file) {
        std::cerr << program_name << ": Missing file before " << argument
                  << "\n";
        usage(program_name);
        return 1;
      }
      input_type = cottontail::meadowlark::InputType::TEXT;
      expecting_file = true;
    } else if (argument == "--code") {
      if (expecting_file) {
        std::cerr << program_name << ": Missing file before " << argument
                  << "\n";
        usage(program_name);
        return 1;
      }
      input_type = cottontail::meadowlark::InputType::CODE;
      expecting_file = true;
    } else if (!argument.empty() && argument[0] == '-') {
      std::cerr << program_name << ": Invalid argument " << argument << "\n";
      usage(program_name);
      return 1;
    } else if (input_type == cottontail::meadowlark::InputType::NONE) {
      std::cerr << program_name << ": Missing input type for " << argument
                << "\n";
      usage(program_name);
      return 1;
    } else {
      std::vector<std::string> filenames;
      if (!cottontail::walk_filesystem(argv[1], &filenames)) {
        std::cerr << program_name << ": Can't walk " << argument << "\n";
        return 1;
      }
      for (const auto &filename : filenames)
        inputs.push_back({input_type, filename});
      expecting_file = false;
    }
    argc--;
    argv++;
  }
  if (expecting_file) {
    std::cerr << program_name << ": Missing file\n";
    usage(program_name);
    return 1;
  }
  if (!cottontail::meadowlark::append_all(warren, inputs, &error)) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }
  return 0;
}
