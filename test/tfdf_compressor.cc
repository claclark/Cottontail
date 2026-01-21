#include "gtest/gtest.h"

#include "src/compressor.h"

TEST(TfdfCompressor, Basic) {
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
  ASSERT_EQ(n, (size_t)0);
  m = compressor->tang(crushed, n, (char *)tanged,
                       sizeof(cottontail::addr) * 1000);
  ASSERT_EQ(m, (size_t)0);
  a = original;
  *a++ = 1;
  *a++ = 1;
  length = (a - original) * sizeof(cottontail::addr);
  n = compressor->crush((char *)original, length, crushed, 1000);
  ASSERT_EQ(n, (size_t)1);
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
}

TEST(TfdfCompressor, Component) {
  cottontail::addr original[100];
  cottontail::addr *a = original;
  for (int i = 1; i < 13; i++) {
    *a++ = i;
    *a++ = 12345;
    *a++ = i + 111;
  }
  size_t length = (a - original) * sizeof(cottontail::addr);
  std::string name = "tfdf";
  std::string recipe = "component";
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
  a = original;
  *a++ = 3129873921873291;
  *a++ = 19191020202;
  *a++ = 6;
  *a++ = 2929292929299;
  length = (a - original) * sizeof(cottontail::addr);
  n = compressor->crush((char *)original, length, crushed, 1000);
  ASSERT_EQ(n, (size_t)0);
}
