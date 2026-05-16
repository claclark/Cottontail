#include <memory>
#include <string>

#include "gtest/gtest.h"

#include "src/json.h"
#include "src/json_featurizer.h"
#include "src/recipe.h"
#include "src/tagging_featurizer.h"

TEST(JsonFeaturizer, Basic) {
  std::shared_ptr<cottontail::Featurizer> featurizer =
      cottontail::JsonFeaturizer::make(cottontail::Featurizer::make("hashing", ""));
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
  EXPECT_EQ(featurizer->featurize(cottontail::open_object_token), 0);
  EXPECT_EQ(featurizer->featurize(cottontail::close_object_token), 0);
  EXPECT_EQ(featurizer->featurize(cottontail::open_array_token), 0);
  EXPECT_EQ(featurizer->featurize(cottontail::close_array_token), 0);
  EXPECT_EQ(featurizer->featurize(cottontail::open_string_token), 0);
  EXPECT_EQ(featurizer->featurize(cottontail::close_string_token), 0);
  EXPECT_EQ(featurizer->featurize(cottontail::colon_token), 0);
  EXPECT_EQ(featurizer->featurize(cottontail::comma_token), 0);
  EXPECT_EQ(featurizer->featurize(cottontail::open_number_token), 0);
  EXPECT_EQ(featurizer->featurize(cottontail::close_number_token), 0);
}

TEST(JsonFeaturizer, IdentityRoundTrip) {
  std::string error;
  std::shared_ptr<cottontail::Featurizer> featurizer =
      cottontail::Featurizer::make("json", "[name:'hashing',recipe:'']",
                                   &error);
  ASSERT_NE(featurizer, nullptr) << error;
  EXPECT_EQ(featurizer->name(), "json");
  std::string wrapped_name;
  std::string wrapped_recipe;
  ASSERT_TRUE(cottontail::unwrap(featurizer->recipe(), &wrapped_name,
                                 &wrapped_recipe, &error))
      << error;
  EXPECT_EQ(wrapped_name, "hashing");
  EXPECT_EQ(wrapped_recipe, "");
  std::shared_ptr<cottontail::Featurizer> round_tripped =
      cottontail::Featurizer::make(featurizer->name(), featurizer->recipe(),
                                   &error);
  ASSERT_NE(round_tripped, nullptr) << error;
  EXPECT_EQ(round_tripped->name(), "json");
  EXPECT_EQ(round_tripped->recipe(), featurizer->recipe());
}

TEST(JsonFeaturizer, TaggingRoundTrips) {
  std::string error;
  std::shared_ptr<cottontail::Featurizer> base =
      cottontail::Featurizer::make("hashing", "", &error);
  ASSERT_NE(base, nullptr) << error;
  std::shared_ptr<cottontail::Featurizer> tagged =
      cottontail::TaggingFeaturizer::make(base, "field", &error);
  ASSERT_NE(tagged, nullptr) << error;
  std::shared_ptr<cottontail::Featurizer> json =
      cottontail::JsonFeaturizer::make(tagged);
  ASSERT_NE(json, nullptr);
  std::shared_ptr<cottontail::Featurizer> round_tripped =
      cottontail::Featurizer::make(json->name(), json->recipe(), &error);
  ASSERT_NE(round_tripped, nullptr) << error;
  EXPECT_EQ(round_tripped->name(), "json");
  EXPECT_EQ(round_tripped->featurize("hello"), json->featurize("hello"));
  EXPECT_EQ(round_tripped->featurize(cottontail::open_object_token), 0);
}
