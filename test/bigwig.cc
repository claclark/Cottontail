#include <memory>
#include <string>

#include "gtest/gtest.h"

#include "src/bigwig.h"
#include "src/cottontail.h"

TEST(bigwig, BASIC) {
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
  hopper->tau(cottontail::minfinity + 1, &p, &q);
  EXPECT_EQ(p, cottontail::maxfinity);
  ASSERT_TRUE(bigwig->appender()->append(std::string(hamlet[6]), &p, &q));
  ASSERT_TRUE(bigwig->annotator()->annotate(line, p, q, (cottontail::addr)7));
  ASSERT_TRUE(bigwig->appender()->append(std::string(hamlet[7]), &p, &q));
  ASSERT_TRUE(bigwig->annotator()->annotate(line, p, q, (cottontail::addr)8));
  ASSERT_TRUE(bigwig->ready());
  bigwig->commit();
  hopper = bigwig->hopper_from_gcl("to");
  int i = 0;
  for (hopper->tau(0, &p, &q); p < cottontail::maxfinity;
       hopper->tau(p + 1, &p, &q)) {
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
  i = 0;
  for (hopper->tau(0, &p, &q); p < cottontail::maxfinity;
       hopper->tau(p + 1, &p, &q)) {
    std::string to = bigwig->txt()->translate(p, q).substr(0, 2);
    EXPECT_TRUE(to == "To" || to == "to");
    i++;
  }
  EXPECT_EQ(i, 8);
}
