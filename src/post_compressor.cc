#include "src/post_compressor.h"

#include <cassert>
#include <cstdint>
#include <string>

namespace cottontail {

namespace {

inline char *encode_1(addr delta, char *out) {
  do {
    char seven = delta & 0x7F;
    delta >>= 7;
    if (delta)
      seven |= 0x80;
    *out++ = seven;
  } while (delta);
  return out;
}

size_t encode_all(addr *list, addr n, char *out) {
  addr *last_addr = list + n;
  char *next_byte = out;
  addr previous_addr = 0;
  for (; list != last_addr; list++) {
    addr current_addr = *list;
    addr current_delta = current_addr - previous_addr;
    next_byte = encode_1(current_delta, next_byte);
    previous_addr = current_addr;
  }
  return next_byte - out;
}

inline char *decode_1(char *in, addr *delta) {
  addr working = 0;
  int shift = 0;
  for (;;) {
    addr current = *in++;
    working |= ((current & 0x7F) << shift);
    if ((current & 0x80) == 0)
      break;
    shift += 7;
  }
  *delta = working;
  return in;
}

size_t decode_all(char *in, size_t length, addr *list, size_t n) {
  addr *current_addr;
  addr *last_addr = list + n;
  char *last_byte = in + length;
  addr value = 0;
  for (current_addr = list; in < last_byte && current_addr < last_addr;
       current_addr++) {
    addr delta;
    in = decode_1(in, &delta);
    value += delta;
    *current_addr = value;
  }
  assert(in == last_byte);
  return current_addr - list;
}

} // namespace

size_t PostCompressor::crush_(char *in, size_t length, char *out,
                              size_t available) {
  addr *list = reinterpret_cast<addr *>(in);
  addr n = length / sizeof(addr);
  return encode_all(list, n, out);
}

size_t PostCompressor::tang_(char *in, size_t length, char *out,
                             size_t available) {
  addr *list = reinterpret_cast<addr *>(out);
  addr n = available / sizeof(addr);
  return sizeof(addr)*decode_all(in, length, list, n);
}

} // namespace cottontail
