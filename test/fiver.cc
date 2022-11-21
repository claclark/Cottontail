#include <memory>
#include <string>

#include "gtest/gtest.h"

#include "src/cottontail.h"
#include "src/fiver.h"
#include "test/fake_text.h"

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

std::shared_ptr<cottontail::Fiver>
make_fiver(std::shared_ptr<cottontail::Fiver> prev, size_t start,
           FakeText *fake) {
  std::shared_ptr<cottontail::Featurizer> featurizer;
  if (prev == nullptr)
    featurizer = cottontail::Featurizer::make("hashing", "");
  else
    featurizer = prev->featurizer();
  if (featurizer == nullptr)
    return nullptr;
  std::shared_ptr<cottontail::Tokenizer> tokenizer;
  if (prev == nullptr)
    tokenizer = cottontail::Tokenizer::make("ascii", "");
  else
    tokenizer = prev->tokenizer();
  if (tokenizer == nullptr)
    return nullptr;
  std::shared_ptr<cottontail::Fiver> fiver =
      cottontail::Fiver::make(nullptr, featurizer, tokenizer);
  if (fiver == nullptr)
    return nullptr;
  if (!fiver->transaction())
    return nullptr;
  fiver->start();
  cottontail::addr p, q;
  cottontail::addr f = featurizer->featurize("fake:");
  if (!fiver->appender()->append(fake->generate(start + 0, start + 127), &p,
                                 &q))
    return nullptr;
  if (!fiver->annotator()->annotate(f, p, q, 1.0))
    return nullptr;
  if (!fiver->appender()->append(fake->generate(start + 128, start + 216), &p,
                                 &q))
    return nullptr;
  if (!fiver->annotator()->annotate(f, p, q, 2.0))
    return nullptr;
  if (!fiver->appender()->append(fake->generate(start + 217, start + 229), &p,
                                 &q))
    return nullptr;
  fiver->annotator()->annotate(f, p, q, 3.0);
  if (prev != nullptr) {
    if (!prev->range(&p, &q))
      return nullptr;
    fiver->relocate(q + 1);
  }
  if (!fiver->ready())
    return nullptr;
  fiver->commit();
  return fiver;
}

void check_text(std::shared_ptr<cottontail::Fiver> fiver, const std::string &s,
                size_t n) {
  std::unique_ptr<cottontail::Hopper> hopper = fiver->hopper_from_gcl(s);
  cottontail::addr p, q;
  size_t i = 0;
  for (hopper->tau(cottontail::minfinity + 1, &p, &q);
       p < cottontail::maxfinity; hopper->tau(p + 1, &p, &q)) {
    std::string t = fiver->txt()->translate(p, q);
    t = "\"" + t + "\"";
    ASSERT_EQ(s, t);
    i++;
  }
  ASSERT_EQ(i, n);
}

TEST(Fiver, Merge) {
  FakeText fake;
  std::vector<std::shared_ptr<cottontail::Fiver>> fivers;
  std::shared_ptr<cottontail::Fiver> fiver0 = make_fiver(nullptr, 10, &fake);
  ASSERT_NE(fiver0, nullptr);
  fivers.push_back(fiver0);
  std::shared_ptr<cottontail::Fiver> fiver1 = make_fiver(fiver0, 20, &fake);
  ASSERT_NE(fiver1, nullptr);
  fivers.push_back(fiver1);
  std::shared_ptr<cottontail::Fiver> fiver2 = make_fiver(fiver1, 30, &fake);
  ASSERT_NE(fiver2, nullptr);
  fivers.push_back(fiver2);
  std::shared_ptr<cottontail::Fiver> fiver = cottontail::Fiver::merge(fivers);
  ASSERT_NE(fiver, nullptr);
  fiver->start();
  std::unique_ptr<cottontail::Hopper> hopper = fiver->hopper_from_gcl("fake:");
  ASSERT_NE(hopper, nullptr);
  cottontail::addr p, q;
  cottontail::fval v;
  hopper->tau(0, &p, &q, &v);
  EXPECT_EQ(p, 0);
  EXPECT_EQ(q, 127);
  EXPECT_EQ(v, 1.0);
  hopper->tau(p + 1, &p, &q, &v);
  EXPECT_EQ(p, 128);
  EXPECT_EQ(q, 216);
  EXPECT_EQ(v, 2.0);
  hopper->tau(p + 1, &p, &q, &v);
  EXPECT_EQ(p, 217);
  EXPECT_EQ(q, 229);
  EXPECT_EQ(v, 3.0);
  hopper->tau(p + 1, &p, &q, &v);
  EXPECT_EQ(p, 230);
  EXPECT_EQ(q, 357);
  EXPECT_EQ(v, 1.0);
  hopper->tau(p + 1, &p, &q, &v);
  EXPECT_EQ(p, 358);
  EXPECT_EQ(q, 446);
  EXPECT_EQ(v, 2.0);
  hopper->tau(p + 1, &p, &q, &v);
  EXPECT_EQ(p, 447);
  EXPECT_EQ(q, 459);
  EXPECT_EQ(v, 3.0);
  hopper->tau(p + 1, &p, &q, &v);
  EXPECT_EQ(p, 460);
  EXPECT_EQ(q, 587);
  EXPECT_EQ(v, 1.0);
  hopper->tau(p + 1, &p, &q, &v);
  EXPECT_EQ(p, 588);
  EXPECT_EQ(q, 676);
  EXPECT_EQ(v, 2.0);
  hopper->tau(p + 1, &p, &q, &v);
  EXPECT_EQ(p, 677);
  EXPECT_EQ(q, 689);
  EXPECT_EQ(v, 3.0);
  check_text(fiver, "\"240\n241\n242\n243\n\"", 2);
  check_text(fiver, "\"238\n239\n20\n21\n\"", 1);
  check_text(fiver, "\"146\n147\n148\n149\n\"", 3);
  check_text(fiver, "\"248\n249\n30\n31\n\"", 1);
  check_text(fiver, "\"136\n137\n138\n139\n\"", 3);
  check_text(fiver, "\"225\n226\n227\n228\n\"", 3);
  fiver->end();
}
