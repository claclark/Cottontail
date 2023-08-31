#include "src/scribe.h"

#include <memory>
#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "src/bigwig.h"
#include "src/cottontail.h"
#include "src/fiver.h"

bool scribe_sonnets(std::shared_ptr<cottontail::Scribe> scribe,
                    std::string *error) {
  std::vector<std::string> sonnets = {"test/sonnet0.txt", "test/sonnet1.txt",
                                      "test/sonnet2.txt"};
  if (cottontail::scribe_files(sonnets, scribe, error, false))
    return scribe->finalize(error);
  else
    return false;
}

void static_warren(std::shared_ptr<cottontail::Warren> *warren) {
  std::string error;
  std::string burrow_name = "sonnet.burrow";
  {
    std::shared_ptr<cottontail::Working> working =
        cottontail::Working::mkdir(burrow_name, &error);
    ASSERT_NE(working, nullptr);
    std::string empty_recipe = "";
    std::string featurizer_name = "hashing";
    std::shared_ptr<cottontail::Featurizer> featurizer =
        cottontail::Featurizer::make(featurizer_name, empty_recipe, &error);
    ASSERT_NE(featurizer, nullptr);
    std::string tokenizer_name = "ascii";
    std::shared_ptr<cottontail::Tokenizer> tokenizer =
        cottontail::Tokenizer::make(tokenizer_name, empty_recipe, &error);
    ASSERT_NE(tokenizer, nullptr);
    std::shared_ptr<cottontail::Builder> builder =
        cottontail::SimpleBuilder::make(working, featurizer, tokenizer, &error);
    ASSERT_NE(builder, nullptr);
    std::shared_ptr<cottontail::Scribe> scribe =
        cottontail::Scribe::make(builder, &error);
    ASSERT_NE(scribe, nullptr);
    EXPECT_TRUE(scribe_sonnets(scribe, &error));
  }
  *warren = cottontail::Warren::make("simple", burrow_name, &error);
  EXPECT_NE(*warren, nullptr) << error;
}

void dynamic_warren(std::shared_ptr<cottontail::Warren> *warren) {
  std::string error;
  std::string empty_recipe = "";
  std::string featurizer_name = "hashing";
  std::shared_ptr<cottontail::Featurizer> featurizer =
      cottontail::Featurizer::make(featurizer_name, empty_recipe, &error);
  ASSERT_NE(featurizer, nullptr);
  std::string tokenizer_name = "ascii";
  std::shared_ptr<cottontail::Tokenizer> tokenizer =
      cottontail::Tokenizer::make(tokenizer_name, empty_recipe, &error);
  ASSERT_NE(tokenizer, nullptr);
  std::shared_ptr<cottontail::Fluffle> fluffle = cottontail::Fluffle::make();
  ASSERT_NE(fluffle, nullptr);
  std::shared_ptr<cottontail::Bigwig> bigwig =
      cottontail::Bigwig::make(nullptr, featurizer, tokenizer, fluffle);
  ASSERT_NE(bigwig, nullptr);
  {
    bigwig->start();
    std::shared_ptr<cottontail::Scribe> scribe =
        cottontail::Scribe::make(bigwig, &error);
    ASSERT_NE(scribe, nullptr);
    EXPECT_TRUE(scribe_sonnets(scribe, &error));
    bigwig->end();
  }
  *warren = bigwig;
}

void check_warren(std::shared_ptr<cottontail::Warren> warren) {
  warren->start();
  cottontail::addr p, q, n;
  std::unique_ptr<cottontail::Hopper> h;
  h = warren->hopper_from_gcl("\"i love\"");
  EXPECT_NE(h, nullptr);
  n = 0;
  for (h->tau(cottontail::minfinity + 1, &p, &q); p < cottontail::maxfinity;
       h->tau(p + 1, &p, &q)) {
    EXPECT_TRUE(warren->txt()->translate(p, q) == std::string("I love "));
    n++;
  }
  EXPECT_EQ(n, 9);
  h = warren->hopper_from_gcl("filename");
  for (h->uat(cottontail::maxfinity - 1, &p, &q); q > cottontail::minfinity;
       h->uat(q - 1, &p, &q))
    EXPECT_TRUE(warren->txt()->translate(p, q).substr(0, 6) ==
                std::string("sonnet"));
  h = warren->hopper_from_gcl("(<< (# 5) (... my eternize))");
  h->rho(cottontail::minfinity + 1, &p, &q);
  EXPECT_TRUE(warren->txt()->translate(p, q) ==
              std::string("My verse, your virtues rare "));
  h->ohr(cottontail::maxfinity - 1, &p, &q);
  EXPECT_TRUE(warren->txt()->translate(p, q) ==
              std::string("your virtues rare shall eternize,\n"));
  warren->end();
}

TEST(Scribe, Static) {
  std::shared_ptr<cottontail::Warren> warren;
  static_warren(&warren);
  check_warren(warren);
}

TEST(Scribe, Dynamic) {
  std::shared_ptr<cottontail::Warren> warren;
  dynamic_warren(&warren);
  check_warren(warren);
}
