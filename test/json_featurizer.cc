#include <memory>
#include <string>

#include "gtest/gtest.h"

#include "src/json.h"
#include "src/json_featurizer.h"

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
