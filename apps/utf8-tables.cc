#include <iostream>
#include <string>

#include "src/cottontail.h"
#include "src/utf8_tokenizer.h"

void usage(std::string program_name) {
  std::cerr << "usage: " << program_name << " [unicode [folding]]";
}

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  if (argc == 2 && argv[1] == std::string("--help")) {
    usage(program_name);
    return 0;
  }
  std::string unicode = "UnicodeData.txt";
  std::string folding = "CaseFolding.txt";
  if (argc > 3) {
    usage(program_name);
    return 1;
  }
  if (argc == 3)
    folding = argv[2];
  if (argc >= 2)
    unicode = argv[1];
  std::string error;
  if (!cottontail::utf8_tables(unicode, folding, &error)) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }
  return 0;
}
