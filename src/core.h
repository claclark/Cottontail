#ifndef COTTONTAIL_SRC_CORE_H_
#define COTTONTAIL_SRC_CORE_H_

// Fundamental definitions and structures.

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace cottontail {

typedef int64_t addr; // Address in feature space
typedef double fval;  // Feature value
constexpr addr maxfinity = INT64_MAX;
constexpr addr minfinity = INT64_MIN;
constexpr addr null_feature = 0;

inline addr fval2addr(fval v) {
  union {
    addr a;
    fval v;
  } u;
  u.v = v;
  return u.a;
}

inline fval addr2fval(addr a) {
  union {
    addr a;
    fval v;
  } u;
  u.a = a;
  return u.v;
}

template <typename T> T &safe_set(T *thing) {
  static T never_used;
  if (thing == nullptr)
    return never_used;
  else
    return *thing;
}

struct safe_error_helper {
  std::string *ptr;
  const char *full_file;
  int line;
  safe_error_helper(std::string *p, const char *f, int l)
      : ptr(p), full_file(f), line(l) {}

  safe_error_helper &operator=(const char *msg) {
    std::string fname(full_file);
    auto pos = fname.find_last_of("/\\");
    if (pos != std::string::npos)
      fname = fname.substr(pos + 1);
    safe_set(ptr) =
        std::string(msg) + " [" + fname + ":" + std::to_string(line) + "]";
    return *this;
  }
  safe_error_helper &operator=(const std::string &msg) {
    std::string fname(full_file);
    auto pos = fname.find_last_of("/\\");
    if (pos != std::string::npos)
      fname = fname.substr(pos + 1);
    safe_set(ptr) =
        std::string(msg) + " [" + fname + ":" + std::to_string(line) + "]";
    return *this;
  }
  operator std::string &() const { return safe_set(ptr); }
};

#define safe_error(ptr) safe_error_helper((ptr), __FILE__, __LINE__)

template <typename T> std::shared_ptr<T> shared_array(addr size) {
  return std::shared_ptr<T>(new T[size], [](T *p) { delete[] p; });
}

bool okay(const std::string &value);
std::string okay(bool yes);
void stamp(std::string label = "");
addr now();
std::vector<std::string> split_re(std::string s, std::string pattern = "\\s+");
std::vector<std::string> split_tsv(const std::string &s,
                                   std::string separator = "\\t");
std::vector<std::string> split_lines(const std::string &s);

struct Token {
  Token() = default;
  Token(addr feature, addr address, size_t offset, size_t length)
      : feature(feature), address(address), offset(offset), length(length){};
  addr feature, address;
  size_t offset, length;
};

struct Annotation {
  Annotation() = default;
  Annotation(addr feature, addr p, addr q, fval v)
      : feature(feature), p(p), q(q), v(v){};
  addr feature, p, q;
  fval v;
};

// If an append doesn't end in one of these character we add a newline.
// Otherwise, we may get a token splice.
inline bool separator(char c) { return c == ' ' || c == '\t' || c == '\n'; }

} // namespace cottontail

#endif // COTTONTAIL_SRC_CORE_H_
