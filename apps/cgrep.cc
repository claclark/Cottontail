#include <iostream>
#include <limits>
#include <memory>
#include <string>

#include "regexp/cgrep.h"
#include "regexp/haystack.h"
#include "src/core.h"
#include "src/nlohmann.h"

namespace {

enum class OutputMode { LINES, RAW };

struct OutputPolicy {
  OutputMode mode = OutputMode::LINES;
  std::size_t limit = 4;
};

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

void coordinates(const cottontail::regexp::LineCgrep::Match &match,
                 nlohmann::json *record) {
  (*record)["start"] = {{"line", match.start_line},
                        {"position", match.start_position}};
  (*record)["end"] = {{"line", match.end_line},
                      {"position", match.end_position}};
}

bool record(const std::string *filename, cottontail::addr p, cottontail::addr q,
            const std::string *text, const char *text_field,
            const cottontail::regexp::LineCgrep::Match *line_match,
            std::string *output, std::string *error) {
  nlohmann::json ordinary;
  if (filename != nullptr)
    ordinary["file"] = *filename;
  ordinary["p"] = p;
  ordinary["q"] = q;
  if (line_match != nullptr && line_match->has_lines)
    coordinates(*line_match, &ordinary);
  if (text != nullptr)
    ordinary[text_field] = *text;
  try {
    *output = ordinary.dump();
    return true;
  } catch (const nlohmann::json::exception &) {
    // Omit invalid result text, but preserve an invalid filename losslessly.
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
  if (line_match != nullptr && line_match->has_lines)
    coordinates(*line_match, &fallback);
  if (text != nullptr) {
    if (json_string(*text))
      fallback[text_field] = *text;
    else
      fallback["binary"] = true;
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
         << " [--lines n | --raw n] regexp [file...]\n"
            "Search files, or standard input when no files are given.\n"
            "Matches use shortest-substring byte-regexp semantics and are "
            "written as JSON Lines.\n\n"
            "Invalid UTF-8 result text is omitted and marked binary.\n\n"
            "  --lines n  Line mode; report matches with their enclosing "
            "lines, up to n lines\n"
            "             (default: 4; 0 means unlimited)\n"
            "  --raw n    Raw mode; report matches from the byte stream, up "
            "to n bytes\n"
            "             (0 means unlimited)\n"
            "  --help     Show this help\n"
            "  --         End options; the next argument is the "
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

bool write(const std::string *filename, cottontail::addr p, cottontail::addr q,
           const std::string *text, const char *text_field,
           const cottontail::regexp::LineCgrep::Match *line_match, bool *found,
           std::string *error) {
  std::string line;
  if (!record(filename, p, q, text, text_field, line_match, &line, error))
    return false;
  std::cout.write(line.data(), static_cast<std::streamsize>(line.size()));
  std::cout.put('\n');
  if (!std::cout) {
    *error = "Cannot write match record";
    return false;
  }
  *found = true;
  return true;
}

bool search_raw(
    const char *program,
    std::shared_ptr<cottontail::regexp::Cgrep> matcher,
    const std::string *filename, std::size_t limit, bool *found) {
  std::string error;

  cottontail::addr p;
  cottontail::addr q;
  while (matcher->match(&p, &q)) {
    std::string match;
    const std::string *included = nullptr;
    std::size_t match_size = static_cast<std::size_t>(q - p) + 1;
    if (limit == 0 || match_size <= limit) {
      match = matcher->translate(p, q);
      if (!matcher->success(&error))
        break;
      included = &match;
    }
    if (!write(filename, p, q, included, "match", nullptr, found, &error))
      break;
  }
  if (!error.empty() || !matcher->success(&error)) {
    report(program, filename, error);
    return false;
  }
  return true;
}

bool search_lines(
    const char *program,
    std::shared_ptr<cottontail::regexp::LineCgrep> matcher,
    const std::string *filename, bool *found) {
  std::string error;

  cottontail::regexp::LineCgrep::Match match;
  while (matcher->match(&match)) {
    std::string lines;
    const std::string *included = nullptr;
    if (match.has_lines) {
      lines = matcher->translate(match);
      if (!matcher->success(&error))
        break;
      included = &lines;
    }
    if (!write(filename, match.p, match.q, included, "lines", &match, found,
               &error))
      break;
  }
  if (!error.empty() || !matcher->success(&error)) {
    report(program, filename, error);
    return false;
  }
  return true;
}

bool search(const char *program,
            std::shared_ptr<const cottontail::regexp::Cgrep::Machine> machine,
            const std::string *filename, const OutputPolicy &policy,
            bool *found) {
  std::string error;
  if (policy.mode == OutputMode::RAW) {
    std::shared_ptr<cottontail::regexp::Cgrep> matcher;
    if (filename != nullptr)
      matcher = cottontail::regexp::Cgrep::make(machine, *filename, &error);
    else
      matcher = cottontail::regexp::Cgrep::make(
          machine,
          std::shared_ptr<std::istream>(&std::cin, [](std::istream *) {}),
          &error);
    if (matcher == nullptr) {
      report(program, filename, error);
      return false;
    }
    return search_raw(program, std::move(matcher), filename, policy.limit, found);
  }
  std::shared_ptr<cottontail::regexp::LineCgrep> matcher;
  if (filename != nullptr)
    matcher = cottontail::regexp::LineCgrep::make(
        machine, *filename, policy.limit, &error);
  else
    matcher = cottontail::regexp::LineCgrep::make(
        machine,
        std::shared_ptr<std::istream>(&std::cin, [](std::istream *) {}),
        policy.limit, &error);
  if (matcher == nullptr) {
    report(program, filename, error);
    return false;
  }
  return search_lines(program, std::move(matcher),
                      filename, found);
}

} // namespace

int main(int argc, char **argv) {
  OutputPolicy policy;
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
    if (option == "--lines" || option == "--raw") {
      std::size_t limit;
      if (++argument >= argc || !size(argv[argument], &limit)) {
        std::cerr << argv[0] << ": " << option << " needs a count\n";
        help(argv[0], std::cerr);
        return 2;
      }
      policy.mode = option == "--lines" ? OutputMode::LINES : OutputMode::RAW;
      policy.limit = limit;
      argument++;
      continue;
    }
    const std::string lines_prefix = "--lines=";
    const std::string raw_prefix = "--raw=";
    bool lines = option.compare(0, lines_prefix.size(), lines_prefix) == 0;
    bool raw = option.compare(0, raw_prefix.size(), raw_prefix) == 0;
    if (lines || raw) {
      const std::string &prefix = lines ? lines_prefix : raw_prefix;
      std::size_t limit;
      if (!size(option.substr(prefix.size()), &limit)) {
        std::cerr << argv[0] << ": " << prefix.substr(0, prefix.size() - 1)
                  << " needs a count\n";
        help(argv[0], std::cerr);
        return 2;
      }
      policy.mode = lines ? OutputMode::LINES : OutputMode::RAW;
      policy.limit = limit;
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
    if (!search(argv[0], machine, nullptr, policy, &found))
      failed = true;
  } else {
    for (int i = argument; i < argc; i++) {
      std::string filename = argv[i];
      if (!search(argv[0], machine, &filename, policy, &found))
        failed = true;
    }
  }

  if (failed)
    return 2;
  return found ? 0 : 1;
}
