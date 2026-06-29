#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "src/cottontail.h"
#include "src/hazel.h"

namespace {

void usage(const std::string &program_name) {
  std::cerr << "usage: " << program_name << " input-hazel input-hazel...\n";
}

std::string seq2str(cottontail::addr sequence) {
  std::stringstream ss;
  ss.fill('0');
  ss.width(20);
  ss << sequence;
  return ss.str();
}

std::string hazel_name(cottontail::addr start, cottontail::addr end) {
  return "hazel." + seq2str(start) + "." + seq2str(end);
}

std::string sibling_path(const std::string &input, const std::string &name) {
  size_t slash = input.find_last_of('/');
  if (slash == std::string::npos)
    return name;
  return input.substr(0, slash + 1) + name;
}

} // namespace

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  if (argc == 2 && argv[1] == std::string("--help")) {
    usage(program_name);
    return 0;
  }
  if (argc < 3) {
    usage(program_name);
    return 1;
  }

  std::string error;
  std::vector<std::shared_ptr<cottontail::Hazel>> hazels;
  hazels.reserve(argc - 1);
  for (int i = 1; i < argc; i++) {
    std::string filename = argv[i];
    std::shared_ptr<cottontail::Warren> warren =
        cottontail::Warren::make(filename, &error);
    if (warren == nullptr) {
      std::cerr << program_name << ": " << error << "\n";
      return 1;
    }
    std::shared_ptr<cottontail::Hazel> hazel =
        std::dynamic_pointer_cast<cottontail::Hazel>(warren);
    if (hazel == nullptr) {
      std::cerr << program_name << ": not a Hazel: " << filename << "\n";
      return 1;
    }
    hazels.push_back(hazel);
  }

  cottontail::addr start = 0;
  cottontail::addr end = 0;
  cottontail::addr previous_end = 0;
  for (size_t i = 0; i < hazels.size(); i++) {
    cottontail::addr current_start;
    cottontail::addr current_end;
    hazels[i]->get_sequence(&current_start, &current_end);
    if (current_start < 0 || current_end < current_start) {
      std::cerr << program_name << ": Hazel input needs sequence metadata: "
                << argv[i + 1] << "\n";
      return 1;
    }
    if (i == 0) {
      start = current_start;
    } else if (previous_end == cottontail::maxfinity ||
               current_start != previous_end + 1) {
      std::cerr << program_name
                << ": Hazel inputs do not form a complete sequence: "
                << argv[i] << " ends at " << previous_end << ", "
                << argv[i + 1] << " starts at " << current_start << "\n";
      return 1;
    }
    end = current_end;
    previous_end = current_end;
  }
  std::string output = sibling_path(argv[1], hazel_name(start, end));
  cottontail::addr t0 = cottontail::now();
  std::shared_ptr<cottontail::Hazel> merged =
      cottontail::Hazel::merge(hazels, output, nullptr, &error);
  if (merged == nullptr) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }
  cottontail::addr t1 = cottontail::now();
  std::cerr << program_name << ": merged " << hazels.size() << " Hazel shards"
            << " into " << output << " in " << (t1 - t0)
            << " millisecond(s)\n";
  return 0;
}
