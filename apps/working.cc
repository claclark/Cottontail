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

#include <fstream>
#include <memory>
#include <string>

#include "src/cottontail.h"

void empty() {
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
  std::string test0 = warren->txt()->translate(0, 10);
  warren->end();
}

class FakeText {
public:
  FakeText(){};
  ~FakeText(){};
  FakeText(const FakeText &) = delete;
  FakeText &operator=(const FakeText &) = delete;
  FakeText(FakeText &&) = delete;
  FakeText &operator=(FakeText &&) = delete;

  std::string text(size_t tokens) {
    std::string text = "";
    for (; tokens > 0; --tokens) {
      if (text != "") {
        if (next_ % 12 == 0)
          text += "\n";
        else
          text += " ";
      }
      text += std::to_string(next_++);
    }
    return text;
  };

private:
  size_t next_ = 0;
};

void test0() {
  std::string error;
  std::string burrow = cottontail::DEFAULT_BURROW;
  std::shared_ptr<cottontail::Working> working =
      cottontail::Working::mkdir(burrow, &error);
  EXPECT_NE(working, nullptr);
  std::shared_ptr<cottontail::Featurizer> featurizer =
      cottontail::Featurizer::make("null", "", &error, working);
  EXPECT_NE(featurizer, nullptr);
  std::shared_ptr<cottontail::Tokenizer> tokenizer =
      cottontail::Tokenizer::make("ascii", "noxml", &error);
  EXPECT_NE(tokenizer, nullptr);
  std::shared_ptr<cottontail::Annotator> annotator =
      cottontail::Annotator::make("null", "", &error, working);
  EXPECT_NE(annotator, nullptr);
  std::string appender_name = "simple";
  std::string appender_recipe = "["
                                "  compressor:\"zlib\","
                                "  compressor_recipe:\"\","
                                "]";
  std::shared_ptr<cottontail::Appender> appender =
      cottontail::Appender::make(appender_name, appender_recipe, &error,
                                 working, featurizer, tokenizer, annotator);
  EXPECT_NE(appender, nullptr);
  EXPECT_TRUE(appender->transaction());
  FakeText fake;
  cottontail::addr p, q;
  EXPECT_TRUE(appender->append(fake.text(101), &p, &q));
  EXPECT_TRUE(appender->append(fake.text(999), &p, &q));
  EXPECT_TRUE(appender->ready());
  appender->commit();
}

int main(int argc, char **argv) {
  empty();
  test0();
  std::string error;
  std::string burrow = cottontail::DEFAULT_BURROW;
  std::string simple = "simple";
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::Warren::make(simple, burrow, &error);
  warren->start();
  std::cout << warren->txt()->translate(8, 15) << "\n";
  std::cout << warren->txt()->translate(42, 64) << "\n";
  std::cout << warren->txt()->translate(99, 128) << "\n";
  warren->end();
  return 0;
}
