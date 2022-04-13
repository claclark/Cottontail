#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "src/cottontail.h"
#include "src/hashing_featurizer.h"
#include "src/tagging_featurizer.h"
#include "src/vocab_featurizer.h"

TEST(VocabFeaturizer, VocabBeforeTag) {
  std::string error;
  std::string name = "vocab";
  std::string recipe =
      "[name:'tagging', recipe:[name:'hashing', tag:'hello'],]";
  ASSERT_TRUE(cottontail::Featurizer::check(name, recipe));
  std::string burrow_name = "test.burrow";
  std::shared_ptr<cottontail::Working> working =
      cottontail::Working::mkdir(burrow_name, &error);
  ASSERT_NE(working, nullptr);
  working->remove(cottontail::VOCAB_NAME);
  static std::shared_ptr<cottontail::Featurizer> featurizer =
      cottontail::Featurizer::make(name, recipe, &error, working);
  ASSERT_NE(featurizer, nullptr);
  std::string s_the = "the";
  std::string s_cat = "cat";
  std::string s_in = "in";
  std::string s_hat = "hat";
  cottontail::addr f_the = featurizer->featurize(s_the);
  cottontail::addr f_cat = featurizer->featurize(s_cat);
  cottontail::addr f_in = featurizer->featurize(s_in);
  EXPECT_EQ(featurizer->featurize(s_the), f_the);
  cottontail::addr f_hat = featurizer->featurize(s_hat);
  EXPECT_EQ(featurizer->featurize(s_in), f_in);
  EXPECT_EQ(featurizer->featurize(s_the), f_the);
  featurizer = nullptr; // force destructor
  std::string vocab_filename = working->make_name(cottontail::VOCAB_NAME);
  std::ifstream vf(vocab_filename);
  ASSERT_TRUE(vf.is_open());
  std::vector<std::string> contents;
  std::string line;
  while (std::getline(vf, line))
    contents.push_back(line);
  vf.close();
  std::sort(contents.begin(), contents.end());
  EXPECT_EQ(contents[0], "3401984639583775598 1 hat");
  EXPECT_EQ(contents[1], "3570349419202381894 2 in");
  EXPECT_EQ(contents[2], "6563246511431673955 3 the");
  EXPECT_EQ(contents[3], "7752353018252607658 1 cat");
  featurizer = cottontail::Featurizer::make(name, recipe, &error, working);
  EXPECT_EQ(featurizer->featurize(s_the), f_the);
  EXPECT_EQ(featurizer->featurize(s_cat), f_cat);
  EXPECT_EQ(featurizer->featurize(s_in), f_in);
  EXPECT_EQ(featurizer->featurize(s_hat), f_hat);
}

TEST(VocabFeaturizer, TagBeforeVocab) {
  std::string error;
  std::string name = "tagging";
  std::string recipe = "[name:'vocab', tag:'hello', recipe:'hashing',]";
  ASSERT_TRUE(cottontail::Featurizer::check(name, recipe));
  std::string burrow_name = "the.burrow";
  std::shared_ptr<cottontail::Working> working =
      cottontail::Working::make(burrow_name, &error);
  working->remove(cottontail::VOCAB_NAME);
  ASSERT_NE(working, nullptr);
  static std::shared_ptr<cottontail::Featurizer> featurizer =
      cottontail::Featurizer::make(name, recipe, &error, working);
  ASSERT_NE(featurizer, nullptr);
  std::string s_the = "the";
  std::string s_cat = "cat";
  std::string s_in = "in";
  std::string s_hat = "hat";
  cottontail::addr f_the = featurizer->featurize(s_the);
  cottontail::addr f_cat = featurizer->featurize(s_cat);
  cottontail::addr f_in = featurizer->featurize(s_in);
  EXPECT_EQ(featurizer->featurize(s_the), f_the);
  cottontail::addr f_hat = featurizer->featurize(s_hat);
  EXPECT_EQ(featurizer->featurize(s_in), f_in);
  EXPECT_EQ(featurizer->featurize(s_the), f_the);
  featurizer = nullptr; // force destructor
  std::string vocab_filename = working->make_name(cottontail::VOCAB_NAME);
  std::ifstream vf(vocab_filename);
  ASSERT_TRUE(vf.is_open());
  std::vector<std::string> contents;
  std::string line;
  while (std::getline(vf, line))
    contents.push_back(line);
  vf.close();
  std::sort(contents.begin(), contents.end());
  EXPECT_EQ(contents[0], "3401984639583775598 1 hello:hat");
  EXPECT_EQ(contents[1], "3570349419202381894 2 hello:in");
  EXPECT_EQ(contents[2], "6563246511431673955 3 hello:the");
  EXPECT_EQ(contents[3], "7752353018252607658 1 hello:cat");
  featurizer = cottontail::Featurizer::make(name, recipe, &error, working);
  EXPECT_EQ(featurizer->featurize(s_the), f_the);
  EXPECT_EQ(featurizer->featurize(s_cat), f_cat);
  EXPECT_EQ(featurizer->featurize(s_in), f_in);
  EXPECT_EQ(featurizer->featurize(s_hat), f_hat);
}
