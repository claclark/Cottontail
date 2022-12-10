#include <memory>
#include <string>

#include "gtest/gtest.h"

#include "src/bigwig.h"
#include "src/cottontail.h"

void basic(bool merge) {
  const char *hamlet[] = {"To be, or not to be, that is the question:",
                          "Whether 'tis nobler in the mind to suffer",
                          "The slings and arrows of outrageous fortune,",
                          "Or to take arms against a sea of troubles",
                          "And by opposing end them. To die-to sleep,",
                          "No more; and by a sleep to say we end",
                          "The heart-ache and the thousand natural shocks",
                          "That flesh is heir to: 'tis a consummation",
                          "Devoutly to be wish'd. To die, to sleep;",
                          "To sleep, perchance to dream-ay, there's the rub:",
                          "For in that sleep of death what dreams may come,"};
  std::shared_ptr<cottontail::Featurizer> featurizer =
      cottontail::Featurizer::make("hashing", "");
  ASSERT_NE(featurizer, nullptr);
  std::shared_ptr<cottontail::Tokenizer> tokenizer =
      cottontail::Tokenizer::make("ascii", "");
  ASSERT_NE(tokenizer, nullptr);
  cottontail::addr p, q, v;
  std::unique_ptr<cottontail::Hopper> hopper;
  cottontail::addr line = featurizer->featurize("line:");
  std::shared_ptr<cottontail::Fluffle> fluffle = cottontail::Fluffle::make();
  std::shared_ptr<cottontail::Bigwig> bigwig =
      cottontail::Bigwig::make(nullptr, featurizer, tokenizer, fluffle);
  bigwig->merge(merge);
  ASSERT_TRUE(bigwig->transaction());
  ASSERT_TRUE(bigwig->appender()->append(std::string(hamlet[0]), &p, &q));
  ASSERT_TRUE(bigwig->annotator()->annotate(line, p, q, (cottontail::addr)1));
  ASSERT_TRUE(bigwig->appender()->append(std::string(hamlet[1]), &p, &q));
  ASSERT_TRUE(bigwig->annotator()->annotate(line, p, q, (cottontail::addr)2));
  ASSERT_TRUE(bigwig->appender()->append(std::string(hamlet[2]), &p, &q));
  ASSERT_TRUE(bigwig->annotator()->annotate(line, p, q, (cottontail::addr)3));
  ASSERT_TRUE(bigwig->appender()->append(std::string(hamlet[3]), &p, &q));
  ASSERT_TRUE(bigwig->annotator()->annotate(line, p, q, (cottontail::addr)4));
  ASSERT_TRUE(bigwig->ready());
  bigwig->commit();
  bigwig->start();
  hopper = bigwig->hopper_from_gcl("\"to be\"");
  ASSERT_NE(hopper, nullptr);
  hopper->tau(cottontail::minfinity + 1, &p, &q);
  EXPECT_EQ(bigwig->txt()->translate(p, q), "To be, ");
  hopper->uat(cottontail::maxfinity - 1, &p, &q);
  EXPECT_EQ(bigwig->txt()->translate(p, q), "to be, ");
  ASSERT_TRUE(bigwig->transaction());
  ASSERT_TRUE(bigwig->appender()->append(std::string(hamlet[4]), &p, &q));
  ASSERT_TRUE(bigwig->annotator()->annotate(line, p, q, (cottontail::addr)5));
  ASSERT_TRUE(bigwig->appender()->append(std::string(hamlet[5]), &p, &q));
  ASSERT_TRUE(bigwig->annotator()->annotate(line, p, q, (cottontail::addr)6));
  hopper = bigwig->hopper_from_gcl("sleep");
  ASSERT_NE(hopper, nullptr);
  hopper->tau(cottontail::minfinity + 1, &p, &q);
  EXPECT_EQ(p, cottontail::maxfinity);
  ASSERT_TRUE(bigwig->appender()->append(std::string(hamlet[6]), &p, &q));
  ASSERT_TRUE(bigwig->annotator()->annotate(line, p, q, (cottontail::addr)7));
  ASSERT_TRUE(bigwig->appender()->append(std::string(hamlet[7]), &p, &q));
  ASSERT_TRUE(bigwig->annotator()->annotate(line, p, q, (cottontail::addr)8));
  ASSERT_TRUE(bigwig->ready());
  bigwig->commit();
  hopper = bigwig->hopper_from_gcl("to");
  ASSERT_NE(hopper, nullptr);
  cottontail::addr i = 0;
  for (hopper->tau(0, &p, &q); p < cottontail::maxfinity;
       hopper->tau(p + 1, &p, &q)) {
    EXPECT_EQ(p, q);
    std::string to = bigwig->txt()->translate(p, q).substr(0, 2);
    EXPECT_TRUE(to == "To" || to == "to");
    i++;
  }
  EXPECT_EQ(i, 4);
  bigwig->end();
  i = 0;
  for (hopper->tau(0, &p, &q); p < cottontail::maxfinity;
       hopper->tau(p + 1, &p, &q))
    i++;
  EXPECT_EQ(i, 4);
  bigwig->start();
  hopper = bigwig->hopper_from_gcl("to");
  ASSERT_NE(hopper, nullptr);
  i = 0;
  for (hopper->tau(0, &p, &q); p < cottontail::maxfinity;
       hopper->tau(p + 1, &p, &q)) {
    EXPECT_EQ(p, q);
    std::string to = bigwig->txt()->translate(p, q).substr(0, 2);
    EXPECT_TRUE(to == "To" || to == "to");
    i++;
  }
  EXPECT_EQ(i, 8);
  bigwig->end();
  ASSERT_TRUE(bigwig->transaction());
  ASSERT_TRUE(bigwig->appender()->append("spam spam spam", &p, &q));
  bigwig->abort();
  bigwig->start();
  hopper = bigwig->hopper_from_gcl("spam");
  ASSERT_NE(hopper, nullptr);
  hopper->tau(0, &p, &q);
  ASSERT_EQ(p, cottontail::maxfinity);
  bigwig->end();
  ASSERT_TRUE(bigwig->transaction());
  ASSERT_TRUE(bigwig->appender()->append(std::string(hamlet[8]), &p, &q));
  ASSERT_TRUE(bigwig->annotator()->annotate(line, p, q, (cottontail::addr)9));
  ASSERT_TRUE(bigwig->appender()->append(std::string(hamlet[9]), &p, &q));
  ASSERT_TRUE(bigwig->annotator()->annotate(line, p, q, (cottontail::addr)10));
  ASSERT_TRUE(bigwig->appender()->append(std::string(hamlet[10]), &p, &q));
  ASSERT_TRUE(bigwig->annotator()->annotate(line, p, q, (cottontail::addr)11));
  ASSERT_TRUE(bigwig->ready());
  bigwig->commit();
  bigwig->start();
  hopper = bigwig->hopper_from_gcl("to");
  ASSERT_NE(hopper, nullptr);
  i = 0;
  for (hopper->tau(0, &p, &q); p < cottontail::maxfinity;
       hopper->tau(p + 1, &p, &q)) {
    EXPECT_EQ(p, q);
    std::string to = bigwig->txt()->translate(p, q).substr(0, 2);
    EXPECT_TRUE(to == "To" || to == "to");
    i++;
  }
  EXPECT_EQ(i, 13);
  hopper = bigwig->hopper_from_gcl("\"troubles and by\"");
  ASSERT_NE(hopper, nullptr);
  hopper->tau(0, &p, &q);
  EXPECT_EQ(bigwig->txt()->translate(p, q), "troubles\nAnd by ");
  hopper->tau(p + 1, &p, &q);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  hopper = bigwig->hopper_from_gcl("\"to sleep to sleep\"");
  ASSERT_NE(hopper, nullptr);
  hopper->tau(0, &p, &q);
  EXPECT_EQ(bigwig->txt()->translate(p, q), "to sleep;\nTo sleep, ");
  hopper->tau(p + 1, &p, &q);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  hopper = bigwig->hopper_from_gcl("\"to sleep\"");
  ASSERT_NE(hopper, nullptr);
  hopper->tau(0, &p, &q);
  EXPECT_EQ(bigwig->txt()->translate(p, q), "to sleep,\n");
  hopper->tau(p + 1, &p, &q);
  EXPECT_EQ(bigwig->txt()->translate(p, q), "to sleep;\n");
  hopper->tau(p + 1, &p, &q);
  EXPECT_EQ(bigwig->txt()->translate(p, q), "To sleep, ");
  hopper->tau(p + 1, &p, &q);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  hopper = bigwig->idx()->hopper(line);
  i = 0;
  ASSERT_NE(hopper, nullptr);
  for (hopper->tau(0, &p, &q, &v); p < cottontail::maxfinity;
       hopper->tau(p + 1, &p, &q, &v)) {
    std::string a = bigwig->txt()->translate(p, q);
    std::string b = std::string(hamlet[i]) + "\n";
    EXPECT_EQ(a, b);
    i++;
    EXPECT_EQ(i, v);
  }
  EXPECT_EQ(i, 11);
}

TEST(Bigwig, Basic) {
  basic(false);
  basic(true);
}

TEST(Bigwig, Two) {
  std::shared_ptr<cottontail::Featurizer> featurizer =
      cottontail::Featurizer::make("hashing", "");
  ASSERT_NE(featurizer, nullptr);
  std::shared_ptr<cottontail::Tokenizer> tokenizer =
      cottontail::Tokenizer::make("ascii", "");
  ASSERT_NE(tokenizer, nullptr);
  cottontail::addr p, q;
  std::unique_ptr<cottontail::Hopper> hopper;
  std::shared_ptr<cottontail::Fluffle> fluffle = cottontail::Fluffle::make();
  std::shared_ptr<cottontail::Bigwig> big =
      cottontail::Bigwig::make(nullptr, featurizer, tokenizer, fluffle);
  big->merge(false);
  std::shared_ptr<cottontail::Bigwig> wig =
      cottontail::Bigwig::make(nullptr, featurizer, tokenizer, fluffle);
  wig->merge(false);
  ASSERT_NE(big, nullptr);
  ASSERT_NE(wig, nullptr);
  big->start();
  wig->start();
  EXPECT_EQ(big->txt()->translate(0, 100), "");
  EXPECT_EQ(wig->txt()->translate(0, 100), "");
  hopper = big->idx()->hopper(0);
  ASSERT_NE(hopper, nullptr);
  hopper->tau(0, &p, &q);
  EXPECT_EQ(p, cottontail::maxfinity);
  hopper = wig->idx()->hopper(0);
  ASSERT_NE(hopper, nullptr);
  hopper->tau(0, &p, &q);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_TRUE(big->transaction());
  EXPECT_TRUE(wig->transaction());
  EXPECT_TRUE(big->appender()->append("a aa aaa aaaa", &p, &q));
  EXPECT_TRUE(wig->appender()->append("1 11 111 1111", &p, &q));
  EXPECT_TRUE(big->appender()->append("b bb bbb bbbb", &p, &q));
  EXPECT_TRUE(wig->appender()->append("2 22 222 2222", &p, &q));
  ASSERT_TRUE(big->ready());
  ASSERT_TRUE(wig->ready());
  big->commit();
  wig->commit();
  EXPECT_EQ(big->txt()->translate(0, 100), "");
  EXPECT_EQ(wig->txt()->translate(0, 100), "");
  big->end();
  wig->end();
  big->start();
  wig->start();
  EXPECT_EQ(big->txt()->translate(6, 9), "bbb bbbb\n1 11 ");
  EXPECT_EQ(wig->txt()->translate(6, 9), "bbb bbbb\n1 11 ");
  big->end();
  wig->end();
  EXPECT_TRUE(wig->transaction());
  EXPECT_TRUE(wig->appender()->append("spam spam spam spam", &p, &q));
  ASSERT_TRUE(wig->ready());
  wig->abort();
  EXPECT_TRUE(wig->transaction());
  EXPECT_TRUE(big->transaction());
  EXPECT_TRUE(wig->appender()->append("1 22 333 4444", &p, &q));
  EXPECT_TRUE(big->appender()->append("a bb ccc dddd", &p, &q));
  EXPECT_TRUE(wig->appender()->append("55555 666666", &p, &q));
  EXPECT_TRUE(big->appender()->append("eeeee ffffff ggggggg", &p, &q));
  ASSERT_TRUE(wig->ready());
  ASSERT_TRUE(big->ready());
  wig->commit();
  big->commit();
  big->start();
  wig->start();
  EXPECT_EQ(big->txt()->translate(15, 20), "2222\n1 ");
  EXPECT_EQ(wig->txt()->translate(15, 20), "2222\n1 ");
  hopper = big->hopper_from_gcl("a");
  ASSERT_NE(hopper, nullptr);
  hopper->tau(0, &p, &q);
  EXPECT_EQ(p, 0);
  EXPECT_EQ(q, 0);
  hopper->tau(p + 1, &p, &q);
  EXPECT_EQ(p, 26);
  EXPECT_EQ(q, 26);
  hopper->tau(p + 1, &p, &q);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  hopper = wig->hopper_from_gcl("bb");
  ASSERT_NE(hopper, nullptr);
  hopper->tau(0, &p, &q);
  EXPECT_EQ(p, 5);
  EXPECT_EQ(q, 5);
  hopper->tau(p + 1, &p, &q);
  EXPECT_EQ(p, 27);
  EXPECT_EQ(q, 27);
  hopper->tau(p + 1, &p, &q);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  hopper = wig->hopper_from_gcl("22");
  ASSERT_NE(hopper, nullptr);
  hopper->tau(0, &p, &q);
  EXPECT_EQ(p, 13);
  EXPECT_EQ(q, 13);
  hopper->tau(p + 1, &p, &q);
  EXPECT_EQ(p, 21);
  EXPECT_EQ(q, 21);
  hopper->tau(p + 1, &p, &q);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  wig->end();
  big->end();
}
