#include <memory>
#include <string>

#include "gtest/gtest.h"

#include "src/hashing_featurizer.h"
#include "src/json_featurizer.h"
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
  EXPECT_EQ(hello_featurizer->name(), "tagging");
  EXPECT_EQ(hello2_featurizer->name(), "tagging");
  std::shared_ptr<cottontail::Featurizer> hello3_featurizer =
      cottontail::Featurizer::make(hello2_featurizer->name(),
                                   hello2_featurizer->recipe(), &error);
  ASSERT_NE(hello3_featurizer, nullptr) << error;
  EXPECT_EQ(hello3_featurizer->featurize(foo), b);
}

TEST(TaggingFeaturizer, NestedRoundTrip) {
  std::string error;
  std::shared_ptr<cottontail::Featurizer> hashing_featurizer =
      cottontail::Featurizer::make("hashing", "", &error);
  ASSERT_NE(hashing_featurizer, nullptr) << error;
  std::shared_ptr<cottontail::Featurizer> inner_featurizer =
      cottontail::TaggingFeaturizer::make(hashing_featurizer, "inner", &error);
  ASSERT_NE(inner_featurizer, nullptr) << error;
  std::shared_ptr<cottontail::Featurizer> outer_featurizer =
      cottontail::TaggingFeaturizer::make(inner_featurizer, "outer", &error);
  ASSERT_NE(outer_featurizer, nullptr) << error;
  std::shared_ptr<cottontail::Featurizer> round_tripped =
      cottontail::Featurizer::make(outer_featurizer->name(),
                                   outer_featurizer->recipe(), &error);
  ASSERT_NE(round_tripped, nullptr) << error;
  std::string foo = "foo";
  EXPECT_EQ(round_tripped->name(), "tagging");
  EXPECT_EQ(round_tripped->featurize(foo), outer_featurizer->featurize(foo));
  EXPECT_EQ(round_tripped->translate(round_tripped->featurize(foo)),
            outer_featurizer->translate(outer_featurizer->featurize(foo)));
}

TEST(TaggingFeaturizer, JsonBaseRoundTrip) {
  std::string error;
  std::shared_ptr<cottontail::Featurizer> json_featurizer =
      cottontail::Featurizer::make("json", "[name:'hashing',recipe:'']",
                                   &error);
  ASSERT_NE(json_featurizer, nullptr) << error;
  std::shared_ptr<cottontail::Featurizer> tagged_featurizer =
      cottontail::TaggingFeaturizer::make(json_featurizer, "field", &error);
  ASSERT_NE(tagged_featurizer, nullptr) << error;
  std::shared_ptr<cottontail::Featurizer> round_tripped =
      cottontail::Featurizer::make(tagged_featurizer->name(),
                                   tagged_featurizer->recipe(), &error);
  ASSERT_NE(round_tripped, nullptr) << error;
  std::string foo = "foo";
  EXPECT_EQ(round_tripped->name(), "tagging");
  EXPECT_EQ(round_tripped->featurize(foo), tagged_featurizer->featurize(foo));
}
