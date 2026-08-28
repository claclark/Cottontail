#include "src/ngram_featurizer.h"

#include <cstdint>
#include <memory>
#include <string>

#include "src/core.h"
#include "src/json.h"
#include "src/murmur_hash.h"

namespace cottontail {
namespace {

constexpr addr hashing_overflow_feature = 0x6f2d4b18a937c5e1LL;
static_assert(hashing_overflow_feature > null_feature);

enum class Marker { NONE, NGRAM, UNIVERSAL, TRANSLATE };

Marker marker(const char *key, addr length) {
  if (length < 3 || static_cast<unsigned char>(key[0]) != 0xEF ||
      static_cast<unsigned char>(key[1]) != 0xB7)
    return Marker::NONE;
  switch (static_cast<unsigned char>(key[2])) {
  case 0x9A:
    return Marker::NGRAM;
  case 0x9B:
    return Marker::UNIVERSAL;
  case 0x9C:
    return Marker::TRANSLATE;
  default:
    return Marker::NONE;
  }
}

addr hash(const char *key, addr length) {
  union {
    char s[sizeof(addr)];
    uint64_t v;
    addr a;
  } u;
  u.v = murmur_hash_64a(key, length, 588503011);
  if (u.a == minfinity)
    return hashing_overflow_feature;
  u.a = u.a > 0 ? u.a : -u.a;
  u.s[sizeof(addr) - 1] |= 0x1;
  return u.a;
}

addr literal(const char *key, addr length) {
  if (length >= static_cast<addr>(sizeof(addr)))
    return null_feature;
  union {
    char s[sizeof(addr)];
    addr a;
  } u;
  addr i = 0;
  for (; i < length; i++)
    u.s[i] = key[i];
  for (; i < static_cast<addr>(sizeof(addr)); i++)
    u.s[i] = '\0';
  return u.a;
}

int hex_value(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

addr translated(const char *key, addr length) {
  if (length <= 0)
    return null_feature;
  uint64_t value = 0;
  for (const char *p = key; p < key + length; p++) {
    int digit = hex_value(*p);
    if (digit < 0 || value > (static_cast<uint64_t>(maxfinity) - digit) / 16)
      return null_feature;
    value = 16 * value + digit;
  }
  if (value == 0)
    return null_feature;
  union {
    char s[sizeof(addr)];
    uint64_t v;
    addr a;
  } u;
  u.v = value;
  if (u.s[sizeof(addr) - 1] == '\0')
    return null_feature;
  return u.a;
}

std::string hexadecimal(addr feature) {
  static const char hex[] = "0123456789abcdef";
  char digits[2 * sizeof(addr)];
  size_t length = 0;
  uint64_t value = static_cast<uint64_t>(feature);
  do {
    digits[length++] = hex[value & 0x0f];
    value >>= 4;
  } while (value != 0);
  std::string result;
  while (length > 0)
    result += digits[--length];
  return result;
}

} // namespace

addr NGramFeaturizer::featurize_(const char *key, addr length) {
  if (length <= 0)
    return null_feature;
  if (json_internal_token(key, length))
    return null_feature;
  Marker kind = marker(key, length);
  if (kind == Marker::NGRAM)
    return literal(key + ngram_marker.size(), length - ngram_marker.size());
  if (kind == Marker::UNIVERSAL)
    return universal_feature;
  if (kind == Marker::TRANSLATE)
    return translated(key + translate_marker.size(),
                      length - translate_marker.size());
  return hash(key, length);
}

std::string NGramFeaturizer::translate_(addr feature) {
  if (feature < 0)
    return universal_marker;
  union {
    char s[sizeof(addr)];
    addr a;
  } u;
  u.a = feature;
  if (u.s[sizeof(addr) - 1] == '\0')
    return ngram_marker + std::string(u.s);
  return translate_marker + hexadecimal(feature);
}

std::shared_ptr<Featurizer> NGramFeaturizer::make(const std::string &recipe,
                                                  std::string *error) {
  if (recipe == "")
    return std::make_shared<NGramFeaturizer>();
  safe_error(error) = "Can't make NGramFeaturizer from recipe : " + recipe;
  return nullptr;
}

bool NGramFeaturizer::check(const std::string &recipe, std::string *error) {
  if (recipe == "")
    return true;
  safe_error(error) = "Can't make NGramFeaturizer from recipe : " + recipe;
  return false;
}

} // namespace cottontail
