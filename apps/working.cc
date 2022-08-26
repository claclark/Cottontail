#define ASSERT_EQ(a, b) (assert((a) == (b)))
#define ASSERT_NE(a, b) (assert((a) != (b)))
#define ASSERT_TRUE(a) (assert((a)))
#define ASSERT_FALSE(a) (assert(!(a)))
#define EXPECT_EQ(a, b) (assert((a) == (b)))
#define EXPECT_NE(a, b) (assert((a) != (b)))
#define EXPECT_TRUE(a) (assert((a)))
#define EXPECT_FALSE(a) (assert(!(a)))

#include <cstring>
#define EXPECT_STREQ(a, b) (assert(strcmp((a), (b)) == 0))

#include <unistd.h>

#include <fstream>
#include <memory>
#include <string>

#include "src/cottontail.h"

void test0() {
}

int main(int argc, char **argv) {
  test0();
  return 0;
}
