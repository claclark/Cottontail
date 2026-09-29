#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "src/cottontail.h"

namespace {

const std::string DEFAULT_BURROW = "scratch.burrow";
const std::string LINE_FEATURE = "line:";
const std::string FILE_FEATURE = "file:";

void usage(const std::string &program_name) {
  std::cerr << "usage: " << program_name
            << " [--burrow burrow] text-file...\n";
}

bool append_file(std::shared_ptr<cottontail::Bigwig> bigwig,
                 const std::string &filename, std::string *error) {
  std::ifstream in(filename);
  if (in.fail()) {
    cottontail::safe_error(error) = "Can't open text file: " + filename;
    return false;
  }

  if (!bigwig->transaction(error))
    return false;

  cottontail::addr line_feature = bigwig->featurizer()->featurize(LINE_FEATURE);
  cottontail::addr file_feature = bigwig->featurizer()->featurize(FILE_FEATURE);
  cottontail::addr file_p = cottontail::maxfinity;
  cottontail::addr file_q = cottontail::minfinity;
  bool have_tokens = false;

  std::string line;
  while (std::getline(in, line)) {
    cottontail::addr p, q;
    if (!bigwig->appender()->append(line + "\n", &p, &q, error)) {
      bigwig->abort();
      return false;
    }
    if (p <= q) {
      if (!bigwig->annotator()->annotate(line_feature, p, q, error)) {
        bigwig->abort();
        return false;
      }
      file_p = std::min(file_p, p);
      file_q = std::max(file_q, q);
      have_tokens = true;
    }
  }
  if (in.bad()) {
    cottontail::safe_error(error) = "Error reading text file: " + filename;
    bigwig->abort();
    return false;
  }
  if (!have_tokens) {
    cottontail::safe_error(error) = "Text file has no tokens: " + filename;
    bigwig->abort();
    return false;
  }
  if (!bigwig->annotator()->annotate(file_feature, file_p, file_q, error)) {
    bigwig->abort();
    return false;
  }
  if (!bigwig->ready()) {
    cottontail::safe_error(error) = "Unable to ready transaction for: " +
                                    filename;
    bigwig->abort();
    return false;
  }
  bigwig->commit();
  return true;
}

} // namespace

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  if (argc == 2 && argv[1] == std::string("--help")) {
    usage(program_name);
    return 0;
  }

  std::string burrow = DEFAULT_BURROW;
  int arg = 1;
  while (arg < argc) {
    std::string option = argv[arg];
    if ((option == "-b" || option == "--burrow") && arg + 1 < argc) {
      burrow = argv[arg + 1];
      arg += 2;
    } else {
      break;
    }
  }

  if (arg >= argc) {
    usage(program_name);
    return 1;
  }

  std::vector<std::string> filenames;
  for (; arg < argc; arg++)
    filenames.push_back(argv[arg]);

  std::string error;
  std::shared_ptr<cottontail::Bigwig> bigwig =
      cottontail::Bigwig::make(burrow, "", &error);
  if (bigwig == nullptr) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }
  bigwig->merge(false);

  for (auto &filename : filenames) {
    if (!append_file(bigwig, filename, &error)) {
      std::cerr << program_name << ": " << error << "\n";
      return 1;
    }
  }

  std::cerr << program_name << ": wrote " << filenames.size()
            << " unmerged Fiver shard(s) to " << burrow << "\n";
  return 0;
}
