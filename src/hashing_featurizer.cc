#include "src/hashing_featurizer.h"

#include <limits>
#include <string>

#include "src/core.h"
#include "src/featurizer.h"

namespace {

// Copied on 2018-09-18 from https://sites.google.com/site/murmurhash/
// Where at that time it stated: "All code is released to the public domain."
uint64_t MurmurHash64A(const void *key, int len, unsigned int seed) {
  const uint64_t m = 0xc6a4a7935bd1e995;
  const int r = 47;

  uint64_t h = seed ^ (len * m);

  const uint64_t *data = (const uint64_t *)key;
  const uint64_t *end = data + (len / 8);

  while (data != end) {
    uint64_t k = *data++;

    k *= m;
    k ^= k >> r;
    k *= m;

    h ^= k;
    h *= m;
  }

  const unsigned char *data2 = (const unsigned char *)data;

  switch (len & 7) {
  case 7:
    h ^= uint64_t(data2[6]) << 48;
  case 6:
    h ^= uint64_t(data2[5]) << 40;
  case 5:
    h ^= uint64_t(data2[4]) << 32;
  case 4:
    h ^= uint64_t(data2[3]) << 24;
  case 3:
    h ^= uint64_t(data2[2]) << 16;
  case 2:
    h ^= uint64_t(data2[1]) << 8;
  case 1:
    h ^= uint64_t(data2[0]);
    h *= m;
  };

  h ^= h >> r;
  h *= m;
  h ^= h >> r;

  return h;
}
} // namespace

namespace cottontail {
namespace {

addr try_digit(char c) {
  switch (c) {
  case '0':
    return 0;
  case '1':
    return 1;
  case '2':
    return 2;
  case '3':
    return 3;
  case '4':
    return 4;
  case '5':
    return 5;
  case '6':
    return 6;
  case '7':
    return 7;
  case '8':
    return 8;
  case '9':
    return 9;
  default:
    return minfinity;
  }
}

addr try_literal(const char *s, addr length) {
  if (length <= 0)
    return minfinity;
  union {
    uint64_t v;
    addr a;
  } u;
  u.v = 0;
  for (const char *p = s; p < s + length; p++) {
    addr digit = try_digit(*p);
    if (digit < 0)
      return minfinity;
    if (u.v < maxfinity / 10 ||
        (u.v == maxfinity / 10 && digit <= maxfinity % 10))
      u.v = 10 * u.v + digit;
    else
      return minfinity;
  }
  return u.a;
}
} // namespace

std::string HashingFeaturizer::recipe_() { return ""; }

addr HashingFeaturizer::featurize_(const char *key, addr length) {
  if (length <= 0)
    return null_feature;
  if (*key == '#') {
    addr literal = try_literal(key + 1, length - 1);
    if (literal >= 0)
      return literal;
  }
  union {
    char s[sizeof(addr)];
    uint64_t v;
    addr a;
  } u;
  if (length < static_cast<addr>(sizeof(addr))) {
    int i = 0;
    for (; i < length; i++)
      u.s[i] = key[i];
    for (; i < static_cast<addr>(sizeof(addr)); i++)
      u.s[i] = '\0';
  } else {
    u.v = MurmurHash64A(key, length, 588503011);
    u.a = u.a > 0 ? u.a : -u.a;
    u.s[sizeof(addr) - 1] |= 0x1;
  }
  return u.a;
}

std::string HashingFeaturizer::translate_(addr feature) {
  if (feature == null_feature)
    return "#0";
  union {
    char s[sizeof(addr)];
    addr a;
  } u;
  u.a = feature;
  if (u.s[sizeof(addr) - 1] != '\0')
    return "#" + std::to_string(u.a);
  else
    return std::string(u.s);
}

std::shared_ptr<Featurizer> HashingFeaturizer::make(const std::string &recipe,
                                                    std::string *error) {
  if (recipe == "") {
    return std::make_shared<HashingFeaturizer>();
  } else {
    safe_error(error) = "Can't make HashingFeaturizer from recipe : " + recipe;
    return nullptr;
  }
}

bool HashingFeaturizer::check(const std::string &recipe, std::string *error) {
  if (recipe == "") {
    return true;
  } else {
    safe_error(error) = "Can't make HashingFeaturizer from recipe : " + recipe;
    return false;
  }
}
} // namespace cottontail
