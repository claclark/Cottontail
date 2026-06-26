#include <memory>
#include <string>

#include "meadowlark/forager.h"
#include "meadowlark/meadowlark.h"
#include "src/cottontail.h"

void usage(std::string program_name) {
  std::cerr << "usage: " << program_name << " [--meadow meadow] [--create]"
            << " [--tsv file...] [--jsonl|--json file...]...\n";
}

enum class InputType { NONE, TSV, JSONL };

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
  std::shared_ptr<cottontail::Warren> warren;
  if (argv[1] == std::string("-c") || argv[1] == std::string("--create")) {
    if (meadow == "")
      warren = cottontail::meadowlark::create_meadow(&error);
    else
      warren = cottontail::meadowlark::create_meadow(meadow, &error);
    --argc;
    argv++;
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
  InputType input_type = InputType::NONE;
  bool expecting_file = false;
  while (argc > 1) {
    std::string argument = argv[1];
    if (argument == "--tsv") {
      if (expecting_file) {
        std::cerr << program_name << ": Missing file before " << argument
                  << "\n";
        usage(program_name);
        return 1;
      }
      input_type = InputType::TSV;
      expecting_file = true;
    } else if (argument == "--jsonl" || argument == "--json") {
      if (expecting_file) {
        std::cerr << program_name << ": Missing file before " << argument
                  << "\n";
        usage(program_name);
        return 1;
      }
      input_type = InputType::JSONL;
      expecting_file = true;
    } else if (!argument.empty() && argument[0] == '-') {
      std::cerr << program_name << ": Invalid argument " << argument << "\n";
      usage(program_name);
      return 1;
    } else if (input_type == InputType::NONE) {
      std::cerr << program_name << ": Missing input type for " << argument
                << "\n";
      usage(program_name);
      return 1;
    } else if (input_type == InputType::TSV) {
      if (!cottontail::meadowlark::append_tsv(warren, argument, &error)) {
        std::cerr << program_name << ": " << error << "\n";
        return 1;
      }
      expecting_file = false;
    } else if (input_type == InputType::JSONL) {
      if (!cottontail::meadowlark::append_jsonl(warren, argument, &error)) {
        std::cerr << program_name << ": " << error << "\n";
        return 1;
      }
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
  return 0;
}
