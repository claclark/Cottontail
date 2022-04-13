#include <memory>
#include <string>

#include "gtest/gtest.h"

#include "src/hashing_featurizer.h"

TEST(HashingFeaturizer, Basic) {
  std::shared_ptr<cottontail::Featurizer> featurizer =
      cottontail::HashingFeaturizer::make();
  std::string recipe = "";
  EXPECT_TRUE(cottontail::HashingFeaturizer::check(recipe));
  recipe = "bad";
  EXPECT_FALSE(cottontail::HashingFeaturizer::check(recipe));
  EXPECT_EQ(featurizer->translate(featurizer->featurize("hello")), "hello");
  EXPECT_NE(featurizer->translate(featurizer->featurize("hello")), "world");
  EXPECT_EQ(featurizer->featurize("hello world"), 6171124712983170968);
  EXPECT_EQ(featurizer->translate(featurizer->featurize("hello world")),
            "#6171124712983170968");
  EXPECT_EQ(
      featurizer->translate(featurizer->featurize("#6171124712983170968")),
      "#6171124712983170968");
  EXPECT_EQ(featurizer->featurize(""), 0);
  EXPECT_EQ(featurizer->featurize("#0"), 0);
}
