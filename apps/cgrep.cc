#include <iostream>
#include <limits>
#include <memory>
#include <string>

#include "regexp/cgrep.h"
#include "regexp/haystack.h"
#include "src/core.h"
#include "src/nlohmann.h"

namespace {

std::string base64(const std::string &input) {
  static const char alphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string output;
  output.reserve(4 * ((input.size() + 2) / 3));
  for (std::size_t i = 0; i < input.size(); i += 3) {
    unsigned int value = static_cast<unsigned char>(input[i]) << 16;
    if (i + 1 < input.size())
      value |= static_cast<unsigned char>(input[i + 1]) << 8;
    if (i + 2 < input.size())
      value |= static_cast<unsigned char>(input[i + 2]);
    output += alphabet[(value >> 18) & 0x3f];
    output += alphabet[(value >> 12) & 0x3f];
    output += i + 1 < input.size() ? alphabet[(value >> 6) & 0x3f] : '=';
    output += i + 2 < input.size() ? alphabet[value & 0x3f] : '=';
  }
  return output;
}

bool json_string(const std::string &value) {
  try {
    (void)nlohmann::json(value).dump();
    return true;
  } catch (const nlohmann::json::exception &) {
    return false;
  }
}

bool record(const std::string *filename, cottontail::addr p, cottontail::addr q,
            const std::string *match, std::string *output, std::string *error) {
  nlohmann::json ordinary;
  if (filename != nullptr)
    ordinary["file"] = *filename;
  ordinary["p"] = p;
  ordinary["q"] = q;
  if (match != nullptr)
    ordinary["match"] = *match;
  try {
    *output = ordinary.dump();
    return true;
  } catch (const nlohmann::json::exception &) {
    // JSON strings must be UTF-8. Preserve only rejected byte strings through
    // the explicitly marked Base64 fallback.
  }

  nlohmann::json fallback;
  fallback["p"] = p;
  fallback["q"] = q;
  if (filename != nullptr) {
    if (json_string(*filename))
      fallback["file"] = *filename;
    else
      fallback["file_base64"] = base64(*filename);
  }
  if (match != nullptr) {
    if (json_string(*match))
      fallback["match"] = *match;
    else
      fallback["match_base64"] = base64(*match);
  }
  try {
    *output = fallback.dump();
    return true;
  } catch (const nlohmann::json::exception &e) {
    cottontail::safe_error_helper(error, __FILE__, __LINE__) =
        "Cannot serialize match record: " + std::string(e.what());
    return false;
  }
}

void report(const char *program, const std::string *filename,
            const std::string &error) {
  std::cerr << program << ": ";
  if (filename != nullptr)
    std::cerr << *filename << ": ";
  std::cerr << error << "\n";
}

void help(const char *program, std::ostream &output) {
  output << "Usage: " << program
         << " [--max-match bytes] regexp [file...]\n"
            "Search files, or standard input when no files are given.\n"
            "Matches use shortest-substring byte-regexp semantics and are "
            "written as JSON Lines.\n\n"
            "  --max-match bytes  Include matching bytes only when their "
            "length is at most bytes\n"
            "                     (default: 256; 0 means unlimited)\n"
            "  --help             Show this help\n"
            "  --                 End options; the next argument is the "
            "regexp\n";
}

bool size(const std::string &value, std::size_t *answer) {
  if (value.empty())
    return false;
  std::size_t parsed = 0;
  for (char c : value) {
    if (c < '0' || c > '9')
      return false;
    std::size_t digit = static_cast<std::size_t>(c - '0');
    if (parsed > (std::numeric_limits<std::size_t>::max() - digit) / 10)
      return false;
    parsed = parsed * 10 + digit;
  }
  *answer = parsed;
  return true;
}

bool search(const char *program,
            std::shared_ptr<const cottontail::regexp::Cgrep::Machine> machine,
            std::shared_ptr<cottontail::regexp::Haystack> haystack,
            const std::string *filename, std::size_t max_match, bool *found) {
  std::string error;
  std::shared_ptr<cottontail::regexp::Cgrep> matcher =
      cottontail::regexp::Cgrep::make(std::move(machine), std::move(haystack),
                                      &error);
  if (matcher == nullptr) {
    report(program, filename, error);
    return false;
  }

  cottontail::addr p;
  cottontail::addr q;
  while (matcher->match(&p, &q)) {
    std::string match;
    const std::string *included = nullptr;
    std::size_t match_size = static_cast<std::size_t>(q - p) + 1;
    if (max_match == 0 || match_size <= max_match) {
      match = matcher->translate(p, q);
      if (!matcher->success(&error))
        break;
      included = &match;
    }
    std::string line;
    if (!record(filename, p, q, included, &line, &error))
      break;
    std::cout.write(line.data(), static_cast<std::streamsize>(line.size()));
    std::cout.put('\n');
    if (!std::cout) {
      error = "Cannot write match record";
      break;
    }
    *found = true;
  }
  if (!error.empty() || !matcher->success(&error)) {
    report(program, filename, error);
    return false;
  }
  return true;
}

} // namespace

int main(int argc, char **argv) {
  std::size_t max_match = 256;
  int argument = 1;
  while (argument < argc) {
    std::string option = argv[argument];
    if (option == "--help") {
      help(argv[0], std::cout);
      return 0;
    }
    if (option == "--") {
      argument++;
      break;
    }
    if (option == "--max-match") {
      if (++argument >= argc || !size(argv[argument], &max_match)) {
        std::cerr << argv[0] << ": --max-match needs a byte count\n";
        help(argv[0], std::cerr);
        return 2;
      }
      argument++;
      continue;
    }
    const std::string prefix = "--max-match=";
    if (option.compare(0, prefix.size(), prefix) == 0) {
      if (!size(option.substr(prefix.size()), &max_match)) {
        std::cerr << argv[0] << ": --max-match needs a byte count\n";
        help(argv[0], std::cerr);
        return 2;
      }
      argument++;
      continue;
    }
    break;
  }
  if (argument >= argc) {
    help(argv[0], std::cerr);
    return 2;
  }

  std::string error;
  std::shared_ptr<const cottontail::regexp::Cgrep::Machine> machine =
      cottontail::regexp::Cgrep::compile(argv[argument++], &error);
  if (machine == nullptr) {
    report(argv[0], nullptr, error);
    return 2;
  }

  bool found = false;
  bool failed = false;
  if (argument == argc) {
    std::shared_ptr<cottontail::regexp::Haystack> haystack =
        cottontail::regexp::Haystack::make_stdin(&error);
    if (haystack == nullptr) {
      report(argv[0], nullptr, error);
      failed = true;
    } else if (!search(argv[0], machine, std::move(haystack), nullptr,
                       max_match, &found)) {
      failed = true;
    }
  } else {
    for (int i = argument; i < argc; i++) {
      std::string filename = argv[i];
      std::shared_ptr<cottontail::regexp::Haystack> haystack =
          cottontail::regexp::Haystack::make(filename, &error);
      if (haystack == nullptr) {
        report(argv[0], &filename, error);
        failed = true;
        continue;
      }
      if (!search(argv[0], machine, std::move(haystack), &filename, max_match,
                  &found))
        failed = true;
    }
  }

  if (failed)
    return 2;
  return found ? 0 : 1;
}
