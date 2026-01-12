#include "src/core.h"

#include <chrono>
#include <ctime>
#include <iostream>
#include <regex>
#include <string>

namespace cottontail {
bool okay(const std::string &value) {
  return value == "y" || value == "yes" || value == "on" || value == "true" ||
         value == "okay" || value == "ok" || value == "Y" || value == "Yes" ||
         value == "On" || value == "True" || value == "Okay" || value == "Ok" ||
         value == "OK";
}

std::string okay(bool yes) {
  if (yes)
    return "yes";
  else
    return "no";
}

void stamp(std::string label) {
  std::time_t now = std::time(nullptr);
  if (label != "")
    std::cerr << label << ": " << now << "\n" << std::flush;
  else
    std::cerr << now << "\n" << std::flush;
}

addr now() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

std::vector<std::string> split_re(std::string str, std::string pattern) {
  while (str.size() > 0 && (str.back() == '\n' || str.back() == '\r'))
    str.pop_back();
  std::regex r(pattern);
  std::vector<std::string> fields{
      std::sregex_token_iterator(str.begin(), str.end(), r, -1), {}};
  return fields;
}

std::vector<std::string> split_tsv(const std::string &s,
                                   std::string separator) {
  std::vector<std::string> out;

  // If empty separator, split on whitespace
  if (separator.empty()) {
    size_t i = 0, n = s.size();
    while (i < n) {
      while (i < n && std::isspace(static_cast<unsigned char>(s[i])))
        ++i;
      if (i >= n)
        break;
      size_t j = i;
      while (j < n && !std::isspace(static_cast<unsigned char>(s[j])))
        ++j;
      out.emplace_back(s.substr(i, j - i));
      i = j;
    }
  } else {
    size_t pos = 0;
    size_t seplen = separator.size();
    for (;;) {
      size_t next = s.find(separator, pos);
      if (next == std::string::npos) {
        out.emplace_back(s.substr(pos));
        break;
      }
      out.emplace_back(s.substr(pos, next - pos));
      pos = next + seplen;
    }
  }
  return out;
}

std::vector<std::string> split_lines(const std::string &s) {
  std::vector<std::string> out;
  out.reserve(1024);

  const char *p = s.data();
  const char *e = p + s.size();
  const char *line_start = p;
  while (p < e) {
    if (*p == '\n' || *p == '\r') {
      const char *line_end = p;

      // Handle CRLF as a single newline.
      if (*p == '\r' && (p + 1) < e && *(p + 1) == '\n')
        ++p;

      out.emplace_back(line_start, line_end - line_start);
      ++p;
      line_start = p;
    } else {
      ++p;
    }
  }
  if (line_start < e)
    out.emplace_back(line_start, e - line_start);

  return out;
}

} // namespace cottontail
