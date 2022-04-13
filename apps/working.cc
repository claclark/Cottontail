#include <algorithm>
#include <cstring>
#include <iostream>
#include <map>
#include <mutex>
#include <regex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#define ASSERT_EQ(a, b) (assert((a) == (b)))
#define ASSERT_NE(a, b) (assert((a) != (b)))
#define ASSERT_TRUE(a) (assert((a)))
#define ASSERT_FALSE(a) (assert(!(a)))
#define EXPECT_EQ(a, b) (assert((a) == (b)))
#define EXPECT_NE(a, b) (assert((a) != (b)))
#define EXPECT_TRUE(a) (assert((a)))
#define EXPECT_FALSE(a) (assert(!(a)))

#include "src/array_hopper.h"
#include "src/bits.h"
#include "src/compressor.h"
#include "src/core.h"
#include "src/featurizer.h"
#include "src/hopper.h"
#include "src/idx.h"
#include "src/ranking.h"
#include "src/simple_builder.h"
#include "src/simple_idx.h"
#include "src/simple_posting.h"
#include "src/simple_txt.h"
#include "src/tokenizer.h"
#include "src/txt.h"
#include "src/warren.h"
#include "src/working.h"

void usage(std::string program_name) {
  std::cerr << "usage: " << program_name << " [--burrow burrow] \n";
}

namespace cottontail {
inline bool crush_addr(addr a, BitsIn *bin) {
  if (a > 0x80000000)
    return false;
  int i = 0, j = 0;
  for (uint32_t mask = 0x00000001; mask < 0x80000000; mask <<= 1) {
    if (a & mask)
      j = i;
    i++;
  }
  for (; j > 0; --j) {
    bin->push(0);
    bin->push(a & 0x00000001);
    a >>= 1;
  }
  bin->push(1);
  return true;
}

inline addr tang_addr(BitsOut *bout) {
  addr a = 0;
  uint32_t mask = 0x00000001;
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
} // namespace cottontail

int main(int argc, char **argv) {

  cottontail::addr original[100];
  cottontail::addr *a = original;
  for (int i = 1; i < 13; i++) {
    *a++ = i;
    *a++ = 12345;
    *a++ = i + 111;
  }
  size_t length = (a - original) * sizeof(cottontail::addr);
  std::string name = "tfdf";
  std::string recipe = "";
  std::shared_ptr<cottontail::Compressor> compressor =
      cottontail::Compressor::make(name, recipe);
  char crushed[1000];
  size_t n = compressor->crush((char *)original, length, crushed, 1000);
  cottontail::addr tanged[1000];
  size_t m = compressor->tang(crushed, n, (char *)tanged,
                              sizeof(cottontail::addr) * 1000);
  ASSERT_EQ(m, 3 * 12 * sizeof(cottontail::addr));
  a = tanged;
  for (int i = 1; i < 13; i++) {
    ASSERT_EQ(*a++, i);
    ASSERT_EQ(*a++, 12345);
    ASSERT_EQ(*a++, i + 111);
  }
  for (int v = 1; v < 191; v++) {
    a = original;
    for (int i = 0; i < 37; i++)
      *a++ = v;
    length = (a - original) * sizeof(cottontail::addr);
    n = compressor->crush((char *)original, length, crushed, 1000);
    m = compressor->tang(crushed, n, (char *)tanged,
                         sizeof(cottontail::addr) * 1000);
    ASSERT_EQ(m, 37 * sizeof(cottontail::addr));
    a = tanged;
    for (int i = 0; i < 37; i++)
      ASSERT_EQ(*a++, v);
  }
  a = original;
  length = (a - original) * sizeof(cottontail::addr);
  n = compressor->crush((char *)original, length, crushed, 1000);
  ASSERT_EQ(n, 0);
  m = compressor->tang(crushed, n, (char *)tanged,
                       sizeof(cottontail::addr) * 1000);
  ASSERT_EQ(m, 0);
  a = original;
  *a++ = 1;
  *a++ = 1;
  length = (a - original) * sizeof(cottontail::addr);
  n = compressor->crush((char *)original, length, crushed, 1000);
  ASSERT_EQ(n, 1);
  m = compressor->tang(crushed, n, (char *)tanged,
                       sizeof(cottontail::addr) * 1000);
  ASSERT_EQ(m, 2 * sizeof(cottontail::addr));
  a = tanged;
  ASSERT_EQ(*a++, 1);
  ASSERT_EQ(*a++, 1);
  a = original;
  *a++ = 3129873921873291;
  *a++ = 19191020202;
  *a++ = 6;
  *a++ = 2929292929299;
  length = (a - original) * sizeof(cottontail::addr);
  n = compressor->crush((char *)original, length, crushed, 1000);
  m = compressor->tang(crushed, n, (char *)tanged,
                       sizeof(cottontail::addr) * 1000);
  ASSERT_EQ(m, 4 * sizeof(cottontail::addr));
  a = tanged;
  ASSERT_EQ(*a++, 3129873921873291);
  ASSERT_EQ(*a++, 19191020202);
  ASSERT_EQ(*a++, 6);
  ASSERT_EQ(*a++, 2929292929299);
  a = original;
  *a++ = -1;
  *a++ = 0;
  *a++ = -6;
  *a++ = -100;
  length = (a - original) * sizeof(cottontail::addr);
  n = compressor->crush((char *)original, length, crushed, 1000);
  m = compressor->tang(crushed, n, (char *)tanged,
                       sizeof(cottontail::addr) * 1000);
  ASSERT_EQ(m, 4 * sizeof(cottontail::addr));
  a = tanged;
  ASSERT_EQ(*a++, -1);
  ASSERT_EQ(*a++, 0);
  ASSERT_EQ(*a++, -6);
  ASSERT_EQ(*a++, -100);
  cottontail::fval x[3] = {1.1, 2.2, 3.3};
  n = compressor->crush((char *)x, 3 * sizeof(cottontail::fval), crushed, 1000);
  cottontail::fval y[3];
  m = compressor->tang(crushed, n, (char *)y, 3 * sizeof(cottontail::fval));
  ASSERT_EQ(m, 3 * sizeof(cottontail::fval));
  ASSERT_EQ(y[0], x[0]);
  ASSERT_EQ(y[1], x[1]);
  ASSERT_EQ(y[2], x[2]);
  return 0;
}
#if 0
  std::string program_name = argv[0];
  if (argc == 2 && argv[1] == std::string("--help")) {
    usage(program_name);
    return 0;
  }
  std::string burrow = cottontail::DEFAULT_BURROW;
  if (argc > 2 &&
      (argv[1] == std::string("-b") || argv[1] == std::string("--burrow"))) {
    burrow = argv[2];
    argc -= 2;
    argv += 2;
  }
  if (argc != 1) {
    usage(program_name);
    return 1;
  }
  std::string error;
  std::string simple = "simple";
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::Warren::make(simple, burrow, &error);
  if (warren == nullptr) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }
  warren->start();
  std::shared_ptr<cottontail::SimpleIdx> simple_idx =
      std::dynamic_pointer_cast<cottontail::SimpleIdx>(warren->idx());
  std::map<cottontail::fval, cottontail::addr> histogram =
      simple_idx->feature_histogram();
  warren->end();
  for (auto &bucket : histogram)
    std::cout << bucket.second << " " << bucket.first << "\n";
  return 0;
#endif
