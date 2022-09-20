#ifndef COTTONTAIL_SRC_CORE_H_
#define COTTONTAIL_SRC_CORE_H_

// Fundamental definitions and structures.

#include <cstdint>
#include <memory>
#include <string>

namespace cottontail {

typedef int64_t addr; // Address in feature space
typedef double fval;  // Feature value
constexpr addr maxfinity = INT64_MAX;
constexpr addr minfinity = INT64_MIN;

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

template <typename T> std::shared_ptr<T> shared_array(addr size) {
  return std::shared_ptr<T>(new T[size], [](T *p) { delete[] p; });
}

bool okay(const std::string &value);
std::string okay(bool yes);
void stamp(std::string label = "");
addr now();

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
} // namespace cottontail

#endif // COTTONTAIL_SRC_CORE_H_
