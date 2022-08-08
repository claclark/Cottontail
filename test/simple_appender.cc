#include <fstream>
#include <memory>
#include <string>

#include "gtest/gtest.h"

#include "src/cottontail.h"

class FakeText {
public:
  FakeText(){};
  ~FakeText(){};
  FakeText(const FakeText &) = delete;
  FakeText &operator=(const FakeText &) = delete;
  FakeText(FakeText &&) = delete;
  FakeText &operator=(FakeText &&) = delete;

  static std::string generate(size_t start, size_t end) {
    if (start < 0)
      start = 0;
    std::string text = "";
    for (size_t n = start; n <= end; n++) {
      if (n > start)
        text += "\n";
      text += std::to_string(n);
    }
    return text;
  };

  static bool check(std::shared_ptr<cottontail::Warren> warren, size_t u,
                    size_t v) {
    std::string t = warren->txt()->translate(u, v);
    std::string f = FakeText::generate(u, v);
    f += "\n";
    return t == f;
  };

  std::string text(size_t tokens) {
    std::string text;
    if (tokens <= 0) {
      text = "";
    } else {
      text = generate(next_, next_ + (tokens - 1));
      next_ += tokens;
    }
    return text;
  };

private:
  size_t next_ = 0;
};

std::shared_ptr<cottontail::Appender>
make_appender(const std::string &burrow, const std::string &appender_name,
              const std::string appender_recipe, std::string *error) {
  std::shared_ptr<cottontail::Working> working =
      cottontail::Working::mkdir(burrow, error);
  EXPECT_NE(working, nullptr);
  std::shared_ptr<cottontail::Featurizer> featurizer =
      cottontail::Featurizer::make("null", "", error, working);
  EXPECT_NE(featurizer, nullptr);
  std::shared_ptr<cottontail::Tokenizer> tokenizer =
      cottontail::Tokenizer::make("ascii", "noxml", error);
  EXPECT_NE(tokenizer, nullptr);
  std::shared_ptr<cottontail::Annotator> annotator =
      cottontail::Annotator::make("null", "", error, working);
  EXPECT_NE(annotator, nullptr);
  EXPECT_TRUE(cottontail::Appender::check(appender_name, appender_recipe));
  std::shared_ptr<cottontail::Appender> appender =
      cottontail::Appender::make(appender_name, appender_recipe, error, working,
                                 featurizer, tokenizer, annotator);
  return appender;
}

TEST(SimpleAppender, FromEmpty) {
  FakeText fake;
  std::string error;
  std::string burrow = cottontail::DEFAULT_BURROW;
  std::string appender_name = "simple";
  std::string appender_recipe = "["
                                "  compressor:\"zlib\","
                                "  compressor_recipe:\"\","
                                "]";
  {
    std::shared_ptr<cottontail::Working> working =
        cottontail::Working::mkdir(burrow, &error);
    EXPECT_NE(working, nullptr);
    std::string options = "tokenizer:noxml txt:compressor:zlib";
    std::shared_ptr<cottontail::Builder> builder =
        cottontail::SimpleBuilder::make(working, options, &error);
    EXPECT_NE(builder, nullptr);
    EXPECT_TRUE(builder->finalize(&error));
    std::shared_ptr<cottontail::Warren> warren =
        cottontail::Warren::make("simple", burrow, &error);
    ASSERT_NE(warren, nullptr);
    warren->start();
    std::string empty = warren->txt()->translate(10, 20);
    ASSERT_TRUE(empty == "");
    warren->end();
  }
  {
    std::shared_ptr<cottontail::Appender> appender =
        make_appender(burrow, appender_name, appender_recipe, &error);
    EXPECT_NE(appender, nullptr);
    cottontail::addr p, q;
    EXPECT_TRUE(appender->transaction());
    EXPECT_TRUE(appender->append(fake.text(101), &p, &q));
    EXPECT_TRUE(appender->append(fake.text(999), &p, &q));
    EXPECT_TRUE(appender->append(fake.text(9000), &p, &q));
    EXPECT_TRUE(appender->append(fake.text(90000), &p, &q));
    EXPECT_TRUE(appender->ready());
    appender->commit();
  }
  {
    std::shared_ptr<cottontail::Warren> warren =
        cottontail::Warren::make("simple", burrow, &error);
    EXPECT_NE(warren, nullptr);
    warren->start();
    EXPECT_TRUE(FakeText::check(warren, 8, 15));
    EXPECT_TRUE(FakeText::check(warren, 42, 64));
    EXPECT_TRUE(FakeText::check(warren, 99, 128));
    EXPECT_TRUE(FakeText::check(warren, 990, 1010));
    EXPECT_TRUE(FakeText::check(warren, 9990, 10010));
    warren->end();
  }
  {
    std::shared_ptr<cottontail::Appender> appender =
        make_appender(burrow, appender_name, appender_recipe, &error);
    EXPECT_NE(appender, nullptr);
    cottontail::addr p, q;
    EXPECT_TRUE(appender->transaction());
    EXPECT_TRUE(appender->append(fake.text(1000000), &p, &q));
    EXPECT_TRUE(appender->append(fake.text(2000000), &p, &q));
    EXPECT_TRUE(appender->ready());
    appender->commit();
  }
  {
    std::shared_ptr<cottontail::Warren> warren =
        cottontail::Warren::make("simple", burrow, &error);
    EXPECT_NE(warren, nullptr);
    warren->start();
    EXPECT_TRUE(FakeText::check(warren, 1009990, 1010010));
    EXPECT_TRUE(FakeText::check(warren, 99, 128));
    EXPECT_TRUE(FakeText::check(warren, 42, 64));
    EXPECT_TRUE(FakeText::check(warren, 1100050, 1100200));
    EXPECT_TRUE(FakeText::check(warren, 990, 1010));
    EXPECT_TRUE(FakeText::check(warren, 8, 15));
    EXPECT_TRUE(FakeText::check(warren, 3100000, 3100099));
    EXPECT_TRUE(FakeText::check(warren, 9990, 10010));
    warren->end();
  }
  {
    std::shared_ptr<cottontail::Appender> appender =
        make_appender(burrow, appender_name, appender_recipe, &error);
    EXPECT_NE(appender, nullptr);
    cottontail::addr p, q;
    EXPECT_TRUE(appender->transaction());
    EXPECT_TRUE(appender->append(fake.text(899000), &p, &q));
    std::shared_ptr<cottontail::Warren> warren =
        cottontail::Warren::make("simple", burrow, &error);
    EXPECT_NE(warren, nullptr);
    warren->start();
    EXPECT_TRUE(FakeText::check(warren, 1000, 2000));
    std::shared_ptr<cottontail::Appender> appender2 =
        make_appender(burrow, appender_name, appender_recipe, &error);
    EXPECT_NE(appender2, nullptr);
    EXPECT_FALSE(appender2->transaction());
    EXPECT_TRUE(appender->append(fake.text(900), &p, &q));
    EXPECT_TRUE(FakeText::check(warren, 0, 12));
    EXPECT_TRUE(FakeText::check(warren, 2000000, 2000000));
    std::string a = warren->txt()->translate(3100097, 3100104);
    EXPECT_STREQ(a.c_str(), "3100097\n3100098\n3100099\n");
    warren->end();
    EXPECT_TRUE(appender->ready());
    appender->commit();
  }
  {
    std::shared_ptr<cottontail::Warren> warren =
        cottontail::Warren::make("simple", burrow, &error);
    EXPECT_NE(warren, nullptr);
    warren->start();
    EXPECT_TRUE(FakeText::check(warren, 1009990, 1010010));
    EXPECT_TRUE(FakeText::check(warren, 99, 128));
    EXPECT_TRUE(FakeText::check(warren, 42, 64));
    EXPECT_TRUE(FakeText::check(warren, 1100050, 1100200));
    EXPECT_TRUE(FakeText::check(warren, 3100097, 3100104));
    EXPECT_TRUE(FakeText::check(warren, 990, 1010));
    EXPECT_TRUE(FakeText::check(warren, 8, 15));
    EXPECT_TRUE(FakeText::check(warren, 3100000, 3100099));
    EXPECT_TRUE(FakeText::check(warren, 9990, 10010));
    warren->end();
  }
  {
    std::shared_ptr<cottontail::Appender> appender =
        make_appender(burrow, appender_name, appender_recipe, &error);
    EXPECT_NE(appender, nullptr);
    cottontail::addr p, q;
    EXPECT_TRUE(appender->transaction());
    EXPECT_TRUE(appender->append(fake.text(1000000), &p, &q));
    EXPECT_TRUE(appender->append(fake.text(2000000), &p, &q));
    EXPECT_TRUE(appender->ready());
    appender->abort();
  }
  {
    std::shared_ptr<cottontail::Warren> warren =
        cottontail::Warren::make("simple", burrow, &error);
    EXPECT_NE(warren, nullptr);
    warren->start();
    EXPECT_TRUE(FakeText::check(warren, 3100001, 3100096));
    EXPECT_TRUE(FakeText::check(warren, 99, 128));
    EXPECT_TRUE(FakeText::check(warren, 3100097, 3100104));
    EXPECT_TRUE(FakeText::check(warren, 1009990, 1010010));
    EXPECT_TRUE(FakeText::check(warren, 42, 64));
    std::string a = warren->txt()->translate(3999997, 4000003);
    EXPECT_STREQ(a.c_str(), "3999997\n3999998\n3999999\n");
    EXPECT_TRUE(FakeText::check(warren, 1100050, 1100200));
    EXPECT_TRUE(FakeText::check(warren, 990, 1010));
    EXPECT_TRUE(FakeText::check(warren, 9990, 10010));
    EXPECT_TRUE(FakeText::check(warren, 8, 15));
    warren->end();
  }
  {
    std::shared_ptr<cottontail::Appender> appender =
        make_appender(burrow, appender_name, appender_recipe, &error);
    EXPECT_NE(appender, nullptr);
    cottontail::addr p, q;
    EXPECT_TRUE(appender->transaction());
    EXPECT_TRUE(appender->append(fake.text(1000000), &p, &q));
    EXPECT_TRUE(appender->append(fake.text(2000000), &p, &q));
    EXPECT_TRUE(appender->ready());
  }
  {
    std::shared_ptr<cottontail::Warren> warren =
        cottontail::Warren::make("simple", burrow, &error);
    EXPECT_NE(warren, nullptr);
    warren->start();
    std::string a = warren->txt()->translate(3999998, 4000003);
    EXPECT_STREQ(a.c_str(), "3999998\n3999999\n");
  }
  {
    std::shared_ptr<cottontail::Working> working =
        cottontail::Working::make(burrow);
    EXPECT_NE(working, nullptr);
    EXPECT_TRUE(cottontail::Appender::recover(appender_name, appender_recipe,
                                              true, &error, working));
  }
  {
    std::shared_ptr<cottontail::Warren> warren =
        cottontail::Warren::make("simple", burrow, &error);
    EXPECT_NE(warren, nullptr);
    warren->start();
    std::string a = warren->txt()->translate(3999998, 4000003);
    EXPECT_STREQ(a.c_str(),
                 "3999998\n3999999\n7000000\n7000001\n7000002\n7000003\n");
    std::string b = warren->txt()->translate(6999998, 7000003);
    EXPECT_STREQ(b.c_str(), "9999998\n9999999\n");
    warren->end();
  }
  {
    std::shared_ptr<cottontail::Appender> appender =
        make_appender(burrow, appender_name, appender_recipe, &error);
    EXPECT_NE(appender, nullptr);
    cottontail::addr p, q;
    EXPECT_TRUE(appender->transaction());
    EXPECT_TRUE(appender->append(fake.text(1000000), &p, &q));
    EXPECT_TRUE(appender->append(fake.text(2000000), &p, &q));
    EXPECT_TRUE(appender->ready());
  }
  {
    std::shared_ptr<cottontail::Warren> warren =
        cottontail::Warren::make("simple", burrow, &error);
    EXPECT_NE(warren, nullptr);
    warren->start();
    std::string a = warren->txt()->translate(3999998, 4000003);
    EXPECT_STREQ(a.c_str(),
                 "3999998\n3999999\n7000000\n7000001\n7000002\n7000003\n");
    std::string b = warren->txt()->translate(6999998, 7000003);
    EXPECT_STREQ(b.c_str(), "9999998\n9999999\n");
    warren->end();
  }
  {
    std::shared_ptr<cottontail::Working> working =
        cottontail::Working::make(burrow);
    EXPECT_NE(working, nullptr);
    EXPECT_TRUE(cottontail::Appender::recover(appender_name, appender_recipe,
                                              false, &error, working));
  }
  {
    std::shared_ptr<cottontail::Warren> warren =
        cottontail::Warren::make("simple", burrow, &error);
    EXPECT_NE(warren, nullptr);
    warren->start();
    std::string a = warren->txt()->translate(3999998, 4000003);
    EXPECT_STREQ(a.c_str(),
                 "3999998\n3999999\n7000000\n7000001\n7000002\n7000003\n");
    std::string b = warren->txt()->translate(6999998, 7000003);
    EXPECT_STREQ(b.c_str(), "9999998\n9999999\n");
    warren->end();
  }
  {
    std::shared_ptr<cottontail::Appender> appender =
        make_appender(burrow, appender_name, appender_recipe, &error);
    EXPECT_NE(appender, nullptr);
    cottontail::addr p, q;
    EXPECT_TRUE(appender->transaction());
    EXPECT_TRUE(appender->append(fake.text(1000000), &p, &q));
    EXPECT_TRUE(appender->append(fake.text(2000000), &p, &q));
    EXPECT_TRUE(appender->ready());
    appender->commit();
  }
  {
    std::shared_ptr<cottontail::Warren> warren =
        cottontail::Warren::make("simple", burrow, &error);
    EXPECT_NE(warren, nullptr);
    warren->start();
    EXPECT_TRUE(FakeText::check(warren, 3100001, 3100096));
    EXPECT_TRUE(FakeText::check(warren, 99, 128));
    std::string a = warren->txt()->translate(3999998, 4000003);
    EXPECT_STREQ(a.c_str(),
                 "3999998\n3999999\n7000000\n7000001\n7000002\n7000003\n");
    EXPECT_TRUE(FakeText::check(warren, 3100097, 3100104));
    std::string b = warren->txt()->translate(6999998, 7000003);
    EXPECT_STREQ(b.c_str(),
                 "9999998\n9999999\n13000000\n13000001\n13000002\n13000003\n");
    EXPECT_TRUE(FakeText::check(warren, 1009990, 1010010));
    EXPECT_TRUE(FakeText::check(warren, 42, 64));
    warren->end();
  }
}

TEST(SimpleAppender, Appending) {
  FakeText fake;
  std::string error;
  std::string burrow = cottontail::DEFAULT_BURROW;
  std::string appender_name = "simple";
  std::string appender_recipe = "["
                                "  compressor:\"bad\","
                                "  compressor_recipe:\"\","
                                "]";
  {
    std::shared_ptr<cottontail::Working> working =
        cottontail::Working::mkdir(burrow, &error);
    EXPECT_NE(working, nullptr);
    std::string options = "tokenizer:noxml txt:compressor:bad";
    std::shared_ptr<cottontail::Builder> builder =
        cottontail::SimpleBuilder::make(working, options, &error);
    EXPECT_NE(builder, nullptr);
    builder->add_text(fake.text(1000000));
    EXPECT_TRUE(builder->finalize(&error));
  }
  {
    std::shared_ptr<cottontail::Warren> warren =
        cottontail::Warren::make("simple", burrow, &error);
    ASSERT_NE(warren, nullptr);
    warren->start();
    EXPECT_TRUE(FakeText::check(warren, 0, 10));
    EXPECT_TRUE(FakeText::check(warren, 100, 200));
    EXPECT_TRUE(FakeText::check(warren, 999990, 999999));
    warren->end();
  }
  {
    std::shared_ptr<cottontail::Appender> appender =
        make_appender(burrow, appender_name, appender_recipe, &error);
    EXPECT_NE(appender, nullptr);
    cottontail::addr p, q;
    EXPECT_TRUE(appender->transaction());
    EXPECT_TRUE(appender->append(fake.text(1000000), &p, &q));
    EXPECT_TRUE(appender->ready());
    appender->commit();
  }
  {
    std::shared_ptr<cottontail::Warren> warren =
        cottontail::Warren::make("simple", burrow, &error);
    EXPECT_NE(warren, nullptr);
    warren->start();
    EXPECT_TRUE(FakeText::check(warren, 0, 10));
    EXPECT_TRUE(FakeText::check(warren, 100, 200));
    EXPECT_TRUE(FakeText::check(warren, 999990, 999999));
    EXPECT_TRUE(FakeText::check(warren, 999990, 1000008));
    EXPECT_TRUE(FakeText::check(warren, 1999990, 1999999));
    warren->end();
  }
}
