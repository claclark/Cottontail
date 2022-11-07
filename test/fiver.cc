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
      cottontail::Fiver::make(nullptr, featurizer, tokenizer, "test0");
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
                      "First Nations, Inuit and Métis peoples\n"
                      "And fulfil my duties\n"
                      "As a Canadian citizen.";
  cottontail::addr p, q;
  ASSERT_TRUE(fiver->appender()->append(test0, &p, &q));
  ASSERT_EQ(p, 0);
  ASSERT_EQ(q, 24);
  ASSERT_TRUE(fiver->appender()->append(test1, &p, &q));
  ASSERT_EQ(p, 25);
  ASSERT_EQ(q, 34);
  ASSERT_TRUE(fiver->appender()->append(test2, &p, &q));
  ASSERT_EQ(p, 35);
  ASSERT_EQ(q, 62);
  ASSERT_TRUE(fiver->ready());
  fiver->commit();
  std::unique_ptr<cottontail::Hopper> hopper =
      fiver->hopper_from_gcl("\"king of canada\"");
  hopper->tau(0, &p, &q);
  ASSERT_EQ(p, 18);
  ASSERT_EQ(q, 20);
  hopper = fiver->hopper_from_gcl("i");
  hopper->tau(0, &p, &q);
  ASSERT_EQ(p, 0);
  ASSERT_EQ(q, 0);
  hopper->uat(1000, &p, &q);
  ASSERT_EQ(p, 27);
  ASSERT_EQ(q, 27);
  fiver->end();
}
