#include "src/dna.h"

#include <fstream>
#include <memory>
#include <string>

#include "src/core.h"

namespace cottontail {
namespace {
const std::string HEADER =
    "# DO NOT EDIT DNA - DO NOT EDIT DNA - DO NOT EDIT DNA ";
}

bool read_dna(std::shared_ptr<Working> working, std::string *dna,
              std::string *error) {
  std::string dna_filename = working->make_name(DNA_NAME);
  std::fstream in(dna_filename, std::ios::in);
  if (in.fail()) {
    safe_set(error) = "Can't read configuration from: " + dna_filename;
    return false;
  }
  std::string line;
  std::getline(in, line);
  if (line != HEADER) {
    safe_set(error) = "Invalid configuration: " + dna_filename;
    return false;
  }
  *dna = "";
  while (std::getline(in, line))
    *dna += (line + " ");
  return true;
}

bool write_dna(std::shared_ptr<Working> working, const std::string &dna,
               std::string *error) {
  std::string dna_filename = working->make_name(DNA_NAME);
  std::string temp_filename = working->make_temp(".dna");
  std::fstream out(temp_filename, std::ios::out);
  if (out.fail()) {
    safe_set(error) = "Can't write configuration to: " + dna_filename;
    return false;
  }
  out << HEADER << "\n";
  out << dna;
  out.close();
  std::rename(temp_filename.c_str(), dna_filename.c_str());
  return true;
}
} // namespace cottontail
