#include <memory>
#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "src/core.h"
#include "src/featurizer.h"
#include "src/ngram_featurizer.h"
#include "src/ngram_tokenizer.h"
#include "src/tokenizer.h"

namespace {
constexpr size_t structural_token_length = 3;
}

TEST(NGramTokenizer, Recipes) {
  const std::string words[] = {"one", "two",   "three", "four",
                               "five", "six",   "seven"};
  for (size_t i = 0; i < 7; i++) {
    std::string digit(1, static_cast<char>('1' + i));
    EXPECT_TRUE(cottontail::NGramTokenizer::check(words[i]));
    EXPECT_TRUE(cottontail::NGramTokenizer::check(digit));
    EXPECT_EQ(cottontail::NGramTokenizer::make(digit)->recipe(), words[i]);
  }
  EXPECT_EQ(cottontail::NGramTokenizer::make("")->recipe(), "five");
  EXPECT_FALSE(cottontail::NGramTokenizer::check("0"));
  EXPECT_FALSE(cottontail::NGramTokenizer::check("8"));
  EXPECT_FALSE(cottontail::NGramTokenizer::check("383892387427420374"));

  std::string error;
  std::shared_ptr<cottontail::Tokenizer> tokenizer =
      cottontail::Tokenizer::make("ngram", "4", &error);
  ASSERT_NE(tokenizer, nullptr) << error;
  EXPECT_EQ(tokenizer->name(), "ngram");
  EXPECT_EQ(tokenizer->recipe(), "four");
  EXPECT_TRUE(cottontail::Tokenizer::check("ngram", "seven"));
  EXPECT_EQ(cottontail::Tokenizer::make("ngram", "bad", &error), nullptr);
}

TEST(NGramTokenizer, LiteralBytes) {
  std::shared_ptr<cottontail::Tokenizer> tokenizer =
      cottontail::NGramTokenizer::make("four");
  std::shared_ptr<cottontail::Featurizer> featurizer =
      cottontail::NGramFeaturizer::make();
  std::vector<cottontail::Token> tokens =
      tokenizer->tokenize(featurizer, "hello");
  ASSERT_EQ(tokens.size(), size_t{5});
  for (size_t i = 0; i < tokens.size(); i++) {
    EXPECT_EQ(tokens[i].address, static_cast<cottontail::addr>(i));
    EXPECT_EQ(tokens[i].offset, i);
    EXPECT_EQ(tokens[i].length, size_t{1});
  }
  EXPECT_EQ(tokens[0].feature,
            featurizer->featurize(cottontail::ngram_marker + "hell"));
  EXPECT_EQ(tokens[1].feature,
            featurizer->featurize(cottontail::ngram_marker + "ello"));
  for (size_t i = 2; i < tokens.size(); i++) {
    EXPECT_EQ(tokens[i].feature, cottontail::universal_feature);
  }

  std::vector<std::string> split = tokenizer->split("hello");
  ASSERT_EQ(split.size(), size_t{5});
  EXPECT_EQ(split[0], cottontail::ngram_marker + "hell");
  EXPECT_EQ(split[1], cottontail::ngram_marker + "ello");
  EXPECT_EQ(split[2], cottontail::universal_marker);
  EXPECT_EQ(tokenizer->bow("hello"),
            (std::vector<std::string>{cottontail::ngram_marker + "hell",
                                      cottontail::ngram_marker + "ello"}));
  EXPECT_EQ(tokenizer->phrase("hello"), split);
  EXPECT_TRUE(tokenizer->phrase("cat").empty());

  std::string newline = "a\nbc";
  EXPECT_EQ(tokenizer->bow(newline),
            (std::vector<std::string>{cottontail::ngram_marker + newline}));
}

TEST(NGramTokenizer, StructuralNoncharacterBlockIsAtomic) {
  std::shared_ptr<cottontail::Tokenizer> tokenizer =
      cottontail::NGramTokenizer::make("four");
  std::shared_ptr<cottontail::Featurizer> featurizer =
      cottontail::NGramFeaturizer::make();
  std::string reserved_noncharacter = "\xEF\xB7\xAF";
  std::string text = "ab" + reserved_noncharacter + "cdef";

  EXPECT_EQ(tokenizer->count(text), 7);
  EXPECT_EQ(tokenizer->skip(text, 2),
            reserved_noncharacter + "cdef");
  EXPECT_EQ(tokenizer->skip(text, 3), "cdef");

  std::vector<cottontail::Token> tokens =
      tokenizer->tokenize(featurizer, text);
  ASSERT_EQ(tokens.size(), size_t{7});
  EXPECT_EQ(tokens[0].feature, cottontail::universal_feature);
  EXPECT_EQ(tokens[1].feature, cottontail::universal_feature);
  EXPECT_EQ(tokens[2].feature, cottontail::null_feature);
  EXPECT_EQ(tokens[2].offset, size_t{2});
  EXPECT_EQ(tokens[2].length, structural_token_length);
  EXPECT_EQ(tokens[3].offset, 2 + structural_token_length);
  EXPECT_EQ(tokens[3].feature,
            featurizer->featurize(cottontail::ngram_marker + "cdef"));

  EXPECT_EQ(tokenizer->bow(text),
            (std::vector<std::string>{cottontail::ngram_marker + "cdef"}));
  std::vector<std::string> phrase = tokenizer->phrase(text);
  ASSERT_EQ(phrase.size(), size_t{7});
  EXPECT_EQ(phrase[3], cottontail::ngram_marker + "cdef");
  for (size_t i = 0; i < phrase.size(); i++) {
    if (i != 3) {
      EXPECT_EQ(phrase[i], cottontail::universal_marker);
    }
  }
}
