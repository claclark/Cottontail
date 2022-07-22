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

#include <memory>
#include <string>

#include "src/cottontail.h"

int main(int argc, char **argv) {
  std::string error;
  std::string burrow = cottontail::DEFAULT_BURROW;
  std::shared_ptr<cottontail::Working> working =
      cottontail::Working::mkdir(burrow, &error);
  EXPECT_NE(working, nullptr);
  std::string options = "tokenizer:noxml";
  std::shared_ptr<cottontail::Builder> builder =
      cottontail::SimpleBuilder::make(working, options, &error);
  EXPECT_NE(builder, nullptr);
  EXPECT_TRUE(builder->finalize(&error));
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::Warren::make("simple", burrow, &error);
  ASSERT_NE(warren, nullptr);
  warren->start();
  std::string test0 = warren->txt()->translate(0,10);
  warren->end();
  return 0;
}
