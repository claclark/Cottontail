#include <iostream>
#include <string>

#include "src/cottontail.h"
#include "src/utf8_tokenizer.h"

void usage(std::string program_name) {
  std::cerr << "usage: " << program_name << " [unicode [folding [recipe]]]\n";
}

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  if (argc == 2 && argv[1] == std::string("--help")) {
    usage(program_name);
    return 0;
  }
  std::string unicode = "UnicodeData.txt";
  std::string folding = "CaseFolding.txt";
  std::string recipe = "";
  if (argc > 4) {
    usage(program_name);
    return 1;
  }
  if (argc == 4)
    recipe = argv[3];
  if (argc >= 3)
    folding = argv[2];
  if (argc >= 2)
    unicode = argv[1];
  std::string error;
  if (!cottontail::utf8_tables(unicode, folding, recipe, &error)) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }
  return 0;
}
