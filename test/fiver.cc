#include <memory>
#include <string>

#include "gtest/gtest.h"

#include "src/cottontail.h"
#include "src/fiver.h"

TEST(Fiver, Basic) {
  std::shared_ptr<cottontail::Featurizer> featurizer =
      cottontail::Featurizer::make("hashing", "");
  EXPECT_NE(featurizer, nullptr);
  std::shared_ptr<cottontail::Tokenizer> tokenizer =
      cottontail::Tokenizer::make("ascii", "");
  ASSERT_NE(tokenizer, nullptr);
  std::shared_ptr<cottontail::Fiver> fiver =
      cottontail::Fiver::make(nullptr, featurizer, tokenizer);
  fiver->start();
  ASSERT_TRUE(fiver->transaction());
  std::string test0 = "I swear\n"
                      "That I will be faithful\n"
                      "And bear true allegiance\n"
                      "To His Majesty\n"
                      "King Charles the Third\n"
                      "King of Canada\n"
                      "His Heirs and Successors";
  std::string test1 = "And that I will faithfully observe\n"
                      "The laws of Canada";
  std::string test2 = "Including the Constitution\n"
                      "Which recognizes and affirms\n"
                      "The Aboriginal and treaty rights of\n"
                      "First Nations, Inuit and Metis peoples\n"
                      "And fulfil my duties\n"
                      "As a Canadian citizen.";
  cottontail::addr p, q;
  cottontail::addr staging = cottontail::maxfinity / 2;
  ASSERT_TRUE(fiver->appender()->append(test0, &p, &q));
  EXPECT_EQ(p, staging + 0);
  EXPECT_EQ(q, staging + 24);
  ASSERT_TRUE(fiver->appender()->append(test1, &p, &q));
  EXPECT_EQ(p, staging + 25);
  EXPECT_EQ(q, staging + 34);
  ASSERT_TRUE(fiver->appender()->append(test2, &p, &q));
  EXPECT_EQ(p, staging + 35);
  EXPECT_EQ(q, staging + 61);
  ASSERT_TRUE(fiver->ready());
  fiver->commit();
  std::unique_ptr<cottontail::Hopper> hopper =
      fiver->hopper_from_gcl("\"king of canada\"");
  hopper->tau(0, &p, &q);
  EXPECT_EQ(p, 18);
  EXPECT_EQ(q, 20);
  hopper = fiver->hopper_from_gcl("i");
  ASSERT_NE(hopper, nullptr);
  hopper->tau(0, &p, &q);
  EXPECT_EQ(p, 0);
  EXPECT_EQ(q, 0);
  hopper->uat(1000, &p, &q);
  EXPECT_EQ(p, 27);
  EXPECT_EQ(q, 27);
  EXPECT_EQ(fiver->txt()->tokens(), 62);
  EXPECT_EQ(fiver->txt()->translate(10, 16),
            "allegiance\nTo His Majesty\nKing Charles the ");
  EXPECT_EQ(fiver->txt()->translate(0, 24), test0 + "\n");
  EXPECT_EQ(fiver->txt()->translate(25, 34), test1 + "\n");
  EXPECT_EQ(fiver->txt()->translate(35, 61), test2);
  EXPECT_EQ(fiver->txt()->translate(35, 100), test2);
  EXPECT_EQ(fiver->txt()->translate(61, 61), "citizen.");
  EXPECT_EQ(fiver->txt()->translate(61, 100), "citizen.");
  EXPECT_EQ(fiver->txt()->translate(62, 100), "");
  EXPECT_EQ(fiver->txt()->translate(0, 0), "I ");
  EXPECT_EQ(fiver->txt()->translate(0, 2), "I swear\nThat ");
  EXPECT_EQ(fiver->txt()->translate(24, 25), "Successors\nAnd ");
  fiver->end();
}
