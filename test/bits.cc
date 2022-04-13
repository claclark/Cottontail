#include "gtest/gtest.h"

#include "src/bits.h"

TEST(BitsTest, None) {
  char buffer[10];
  cottontail::BitsIn in(&buffer);
  ASSERT_EQ(in.count(), 0);
  cottontail::BitsOut out(in.bits(), in.count());
  ASSERT_TRUE(out.empty());
}

TEST(BitsTest, Three) {
  char buffer[2];
  cottontail::BitsIn in(&buffer);
  in.push(0);
  in.push(1);
  in.push(1);
  in.push(0);
  ASSERT_EQ(in.count(), 4);
  cottontail::BitsOut out(in.bits(), in.count());
  ASSERT_FALSE(out.empty());
  ASSERT_EQ(out.front(), 0);
  out.pop();
  ASSERT_FALSE(out.empty());
  ASSERT_EQ(out.front(), 1);
  out.pop();
  ASSERT_FALSE(out.empty());
  ASSERT_EQ(out.front(), 1);
  out.pop();
  ASSERT_FALSE(out.empty());
  ASSERT_EQ(out.front(), 0);
  out.pop();
  ASSERT_TRUE(out.empty());
}

TEST(BitsTest, Lots) {
  char buffer[1000];
  cottontail::BitsIn in(&buffer);
  for (int i = 0; i < 10; i++) {
    for (int j = 0; j < i + 1; j++)
      in.push(1);
    for (int j = 0; j < i + 1; j++)
      in.push(0);
  }

  cottontail::BitsOut out(in.bits(), in.count());
  for (int i = 0; i < 10; i++) {
    for (int j = 0; j < i + 1; j++) {
      ASSERT_FALSE(out.empty());
      ASSERT_EQ(out.front(), 1);
      out.pop();
    }
    for (int j = 0; j < i + 1; j++) {
      ASSERT_FALSE(out.empty());
      ASSERT_EQ(out.front(), 0);
      out.pop();
    }
  }
  ASSERT_TRUE(out.empty());
}
