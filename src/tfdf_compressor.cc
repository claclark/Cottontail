#include "src/tfdf_compressor.h"

#include <cassert>
#include <cstring>
#include <memory>
#include <string>

#include "src/bits.h"

namespace cottontail {

namespace {

const int BIG = 0x40000000; // don't change

inline bool crush_addr(addr a, BitsIn *bin) {
  if (a <= 0 || a > BIG)
    return false;
  int i = 0, j = 0;
  for (addr mask = 1; mask <= BIG; mask <<= 1) {
    if (a & mask)
      j = i;
    i++;
  }
  for (; j > 0; --j) {
    bin->push(0);
    bin->push(a & 1);
    a >>= 1;
  }
  bin->push(1);
  return true;
}

inline addr tang_addr(BitsOut *bout) {
  addr a = 0;
  addr mask = 1;
  while (bout->front() == 0) {
    bout->pop();
    if (bout->front() == 1)
      a |= mask;
    mask <<= 1;
    bout->pop();
  }
  a |= mask;
  bout->pop();
  return a;
}
} // namespace

size_t TfdfCompressor::crush_(char *in, size_t length, char *out,
                              size_t available) {
  assert(length % (sizeof(addr)) == 0);
  BitsIn b(out);
  for (addr *a = (addr *)in; a < (addr *)in + length / (sizeof(addr)); a++)
    if (!crush_addr(*a, &b)) {
      if (component_)
        return 0;
      else {
        memcpy(out, in, length);
        return length;
      }
    }
  while (b.count() % 8 != 0)
    b.push(0);
  (void) b.bits();
  size_t n = b.count() / 8;
  if (!component_ && n % (sizeof(addr)) == 0)
    out[n++] = '\0';
  return n;
}

size_t TfdfCompressor::tang_(char *in, size_t length, char *out,
                             size_t available) {
  if (!component_) {
    if (length % (sizeof(addr)) == 0) {
      memcpy(out, in, length);
      return length;
    }
    if (in[length - 1] == '\0')
      --length;
  }
  assert(in[length - 1] != '\0');
  addr count = 8 * length;
  unsigned char *uin = (unsigned char *) in;
  unsigned mask = 0x80;
  while((uin[length - 1] & mask) == 0) {
    --count;
    mask >>= 1;
  }
  BitsOut b(uin, count);
  addr *a = (addr *) out;
  while (!b.empty())
    *a++ = tang_addr(&b);
  return (((char *) a) - out);
}

} // namespace cottontail
