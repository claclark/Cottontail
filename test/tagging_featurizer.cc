#include <memory>
#include <string>

#include "gtest/gtest.h"

#include "src/hashing_featurizer.h"
#include "src/tagging_featurizer.h"

TEST(TaggingFeaturizer, Basic) {
  std::string name = "tagging";
  std::string recipe = "[name:'hashing', tag:'hello']";
  std::string error;
  EXPECT_TRUE(cottontail::Featurizer::check(name, recipe, &error));
  std::shared_ptr<cottontail::Featurizer> hello_featurizer =
      cottontail::Featurizer::make(name, recipe, &error);
  ASSERT_NE(hello_featurizer, nullptr);
  std::shared_ptr<cottontail::Featurizer> hashing_featurizer =
      cottontail::HashingFeaturizer::make();
  ASSERT_NE(hashing_featurizer, nullptr);
  std::string hashing = "hashing";
  std::string hello = "hello";
  std::shared_ptr<cottontail::Featurizer> hello2_featurizer =
      cottontail::TaggingFeaturizer::make(hashing_featurizer, hello, &error);
  ASSERT_NE(hello2_featurizer, nullptr);
  std::string foo = "foo";
  cottontail::addr a = hello_featurizer->featurize(foo);
  cottontail::addr b = hello2_featurizer->featurize(foo);
  EXPECT_EQ(a, b);
}
