#define ASSERT_EQ(a, b) (assert((a) == (b)))
#define ASSERT_NE(a, b) (assert((a) != (b)))
#define ASSERT_TRUE(a) (assert((a)))
#define ASSERT_FALSE(a) (assert(!(a)))
#define EXPECT_EQ(a, b) (assert((a) == (b)))
#define EXPECT_NE(a, b) (assert((a) != (b)))
#define EXPECT_GE(a, b) (assert((a) >= (b)))
#define EXPECT_GT(a, b) (assert((a) > (b)))
#define EXPECT_TRUE(a) (assert((a)))
#define EXPECT_FALSE(a) (assert(!(a)))

#include <cmath>
#define EXPECT_FLOAT_EQ(a, b) assert(fabs((a) - (b)) < 0.00000001)

#include <cstring>
#define EXPECT_STREQ(a, b) (assert(strcmp((a), (b)) == 0))
#define ASSERT_STREQ(a, b) (assert(strcmp((a), (b)) == 0))

#define TEST(a, b) void test_##a##_##b()

#include <algorithm>
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

#include "src/cottontail.h"

void usage(std::string program_name) {
  std::cerr << "usage: " << program_name << "\n";
}

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  std::string error;
  return 0;
}
