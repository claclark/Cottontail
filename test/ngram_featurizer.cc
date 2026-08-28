#include <memory>
#include <string>

#include "gtest/gtest.h"

#include "src/core.h"
#include "src/featurizer.h"
#include "src/hashing_featurizer.h"
#include "src/json.h"
#include "src/ngram_featurizer.h"

TEST(NGramFeaturizer, RecipeAndFactory) {
  std::string error;
  EXPECT_TRUE(cottontail::NGramFeaturizer::check("", &error));
  EXPECT_FALSE(cottontail::NGramFeaturizer::check("bad", &error));
  EXPECT_EQ(cottontail::NGramFeaturizer::make("bad", &error), nullptr);
  std::shared_ptr<cottontail::Featurizer> featurizer =
      cottontail::Featurizer::make("ngram", "", &error);
  ASSERT_NE(featurizer, nullptr) << error;
  EXPECT_EQ(featurizer->name(), "ngram");
  EXPECT_EQ(featurizer->recipe(), "");
}

TEST(NGramFeaturizer, Markers) {
  std::shared_ptr<cottontail::Featurizer> featurizer =
      cottontail::NGramFeaturizer::make();
  ASSERT_NE(featurizer, nullptr);

  EXPECT_EQ(featurizer->featurize(""), cottontail::null_feature);
  EXPECT_EQ(featurizer->featurize(cottontail::ngram_marker),
            cottontail::null_feature);
  std::string longest = cottontail::ngram_marker + "1234567";
  EXPECT_NE(featurizer->featurize(longest), cottontail::null_feature);
  EXPECT_EQ(featurizer->translate(featurizer->featurize(longest)), longest);
  EXPECT_EQ(featurizer->featurize(cottontail::ngram_marker + "12345678"),
            cottontail::null_feature);
  EXPECT_EQ(featurizer->featurize(cottontail::universal_marker),
            cottontail::universal_feature);
  EXPECT_EQ(featurizer->featurize(cottontail::universal_marker + "ignored"),
            cottontail::universal_feature);
}

TEST(NGramFeaturizer, JsonStructuralTokensAreNull) {
  std::shared_ptr<cottontail::Featurizer> featurizer =
      cottontail::NGramFeaturizer::make();
  ASSERT_NE(featurizer, nullptr);
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

TEST(NGramFeaturizer, TranslationRoundTrips) {
  std::shared_ptr<cottontail::Featurizer> featurizer =
      cottontail::NGramFeaturizer::make();
  ASSERT_NE(featurizer, nullptr);

  std::string literal = cottontail::ngram_marker + "hello";
  cottontail::addr literal_feature = featurizer->featurize(literal);
  EXPECT_EQ(featurizer->translate(literal_feature), literal);
  EXPECT_EQ(featurizer->featurize(featurizer->translate(literal_feature)),
            literal_feature);
  EXPECT_EQ(featurizer->translate(cottontail::null_feature),
            cottontail::ngram_marker);
  EXPECT_EQ(featurizer->translate(cottontail::universal_feature),
            cottontail::universal_marker);

  cottontail::addr hashed_feature = featurizer->featurize("hello");
  std::string translated = featurizer->translate(hashed_feature);
  EXPECT_EQ(translated.substr(0, cottontail::translate_marker.size()),
            cottontail::translate_marker);
  EXPECT_EQ(featurizer->featurize(translated), hashed_feature);
}

TEST(NGramFeaturizer, BadTranslationsAreNull) {
  std::shared_ptr<cottontail::Featurizer> featurizer =
      cottontail::NGramFeaturizer::make();
  ASSERT_NE(featurizer, nullptr);
  EXPECT_EQ(featurizer->featurize(cottontail::translate_marker), 0);
  EXPECT_EQ(featurizer->featurize(cottontail::translate_marker + "xyz"), 0);
  EXPECT_EQ(featurizer->featurize(cottontail::translate_marker + "0"), 0);
  EXPECT_EQ(featurizer->featurize(cottontail::translate_marker + "1"), 0);
  EXPECT_EQ(featurizer->featurize(cottontail::translate_marker +
                                  "ffffffffffffffff"),
            0);
}

TEST(NGramFeaturizer, HashesMatch) {
  std::shared_ptr<cottontail::Featurizer> ngram =
      cottontail::NGramFeaturizer::make();
  std::shared_ptr<cottontail::Featurizer> hashing =
      cottontail::HashingFeaturizer::make();
  ASSERT_NE(ngram, nullptr);
  ASSERT_NE(hashing, nullptr);
  EXPECT_EQ(ngram->featurize("hello world"),
            hashing->featurize("hello world"));
  EXPECT_NE(ngram->featurize("hello"), hashing->featurize("hello"));
}
