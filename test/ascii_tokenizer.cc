#include <memory>
#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "src/ascii_tokenizer.h"
#include "src/core.h"
#include "src/hashing_featurizer.h"

TEST(AsciiTokenizer, XMLTokenize) {
  std::string recipe = "";
  EXPECT_TRUE(cottontail::AsciiTokenizer::check(recipe));
  recipe = "xml";
  EXPECT_TRUE(cottontail::AsciiTokenizer::check(recipe));
  recipe = "noxml";
  EXPECT_TRUE(cottontail::AsciiTokenizer::check(recipe));
  recipe = "hello";
  EXPECT_FALSE(cottontail::AsciiTokenizer::check(recipe));
  std::shared_ptr<cottontail::Featurizer> featurizer =
      cottontail::HashingFeaturizer::make();
  std::shared_ptr<cottontail::Tokenizer> tokenizer =
      cottontail::AsciiTokenizer::make(true);
  std::vector<cottontail::Token> tokens;
  {
    char text[] = "--)(*&^%$#@!<<<<>>";
    tokens = tokenizer->tokenize(featurizer, text, sizeof(text));
    EXPECT_EQ(tokens.size(), (size_t)0);
  }
  {
    char text[] = "the cat sat on the mat";
    tokens = tokenizer->tokenize(featurizer, text, sizeof(text));
    ASSERT_EQ(tokens.size(), (size_t)6);
    EXPECT_EQ(tokens[1].feature, featurizer->featurize("cat"));
    EXPECT_EQ(tokens[1].address, 1);
    EXPECT_EQ(tokens[1].offset, (size_t)4);
    EXPECT_EQ(tokens[1].length, (size_t)3);
    EXPECT_EQ(tokens[5].feature, featurizer->featurize("mat"));
    EXPECT_EQ(tokens[5].address, 5);
    EXPECT_EQ(tokens[5].offset, (size_t)19);
    EXPECT_EQ(tokens[5].length, (size_t)3);
  }
  {
    char text[] = "^&*the THE <the><Cat>cat<sat> sat, ON the Mat";
    tokens = tokenizer->tokenize(featurizer, text, sizeof(text));
    ASSERT_EQ(tokens.size(), (size_t)12);
    EXPECT_EQ(tokens[1].feature, featurizer->featurize("THE"));
    EXPECT_EQ(tokens[1].address, 1);
    EXPECT_EQ(tokens[1].offset, (size_t)7);
    EXPECT_EQ(tokens[1].length, (size_t)3);
    EXPECT_EQ(tokens[2].feature, featurizer->featurize("the"));
    EXPECT_EQ(tokens[2].address, 1);
    EXPECT_EQ(tokens[2].offset, (size_t)7);
    EXPECT_EQ(tokens[2].length, (size_t)3);
    EXPECT_EQ(tokens[3].feature, featurizer->featurize("<the>"));
    EXPECT_EQ(tokens[3].address, 2);
    EXPECT_EQ(tokens[3].offset, (size_t)11);
    EXPECT_EQ(tokens[3].length, (size_t)5);
    EXPECT_EQ(tokens[4].feature, featurizer->featurize("<Cat>"));
    EXPECT_EQ(tokens[4].address, 3);
    EXPECT_EQ(tokens[4].offset, (size_t)16);
    EXPECT_EQ(tokens[4].length, (size_t)5);
    EXPECT_EQ(tokens[11].feature, featurizer->featurize("mat"));
    EXPECT_EQ(tokens[11].address, 9);
    EXPECT_EQ(tokens[11].offset, (size_t)42);
    EXPECT_EQ(tokens[11].length, (size_t)3);
  }
  {
    char text[] =
        "the --&*((@THE******<the test=14 ><cat>cat<sat> sat on the <mat ";
    tokens = tokenizer->tokenize(featurizer, text, sizeof(text));
    ASSERT_EQ(tokens.size(), (size_t)11);
    EXPECT_EQ(tokens[1].feature, featurizer->featurize("THE"));
    EXPECT_EQ(tokens[1].address, 1);
    EXPECT_EQ(tokens[1].offset, (size_t)11);
    EXPECT_EQ(tokens[1].length, (size_t)3);
    EXPECT_EQ(tokens[2].feature, featurizer->featurize("the"));
    EXPECT_EQ(tokens[2].address, 1);
    EXPECT_EQ(tokens[2].offset, (size_t)11);
    EXPECT_EQ(tokens[2].length, (size_t)3);
    EXPECT_EQ(tokens[3].feature, featurizer->featurize("<the>"));
    EXPECT_EQ(tokens[3].address, 2);
    EXPECT_EQ(tokens[3].offset, (size_t)20);
    EXPECT_EQ(tokens[3].length, (size_t)14);
    EXPECT_EQ(tokens[10].feature, featurizer->featurize("mat"));
    EXPECT_EQ(tokens[10].address, 9);
    EXPECT_EQ(tokens[10].offset, (size_t)60);
    EXPECT_EQ(tokens[10].length, (size_t)3);
  }
  {
    char text[] = "    <hello djslslsjf> <world slslsl> <tag=not closed   ";
    tokens = tokenizer->tokenize(featurizer, text, sizeof(text));
    ASSERT_EQ(tokens.size(), (size_t)5);
    EXPECT_EQ(tokens[0].feature, featurizer->featurize("<hello>"));
    EXPECT_EQ(tokens[0].address, 0);
    EXPECT_EQ(tokens[0].offset, (size_t)4);
    EXPECT_EQ(tokens[0].length, (size_t)17);
    EXPECT_EQ(tokens[2].feature, featurizer->featurize("tag"));
    EXPECT_EQ(tokens[2].address, 2);
    EXPECT_EQ(tokens[2].offset, (size_t)38);
    EXPECT_EQ(tokens[2].length, (size_t)3);
    EXPECT_EQ(tokens[4].feature, featurizer->featurize("closed"));
    EXPECT_EQ(tokens[4].address, 4);
    EXPECT_EQ(tokens[4].offset, (size_t)46);
    EXPECT_EQ(tokens[4].length, (size_t)6);
  }
  {
    char text[] = "  +hello &ignore; &do not ignore ; &&&ignore; tEst=== ";
    tokens = tokenizer->tokenize(featurizer, text, sizeof(text));
    ASSERT_EQ(tokens.size(), (size_t)6);
    EXPECT_EQ(tokens[0].feature, featurizer->featurize("hello"));
    EXPECT_EQ(tokens[0].address, 0);
    EXPECT_EQ(tokens[0].offset, (size_t)3);
    EXPECT_EQ(tokens[0].length, (size_t)5);
    EXPECT_EQ(tokens[3].feature, featurizer->featurize("ignore"));
    EXPECT_EQ(tokens[3].address, 3);
    EXPECT_EQ(tokens[3].offset, (size_t)26);
    EXPECT_EQ(tokens[3].length, (size_t)6);
    EXPECT_EQ(tokens[4].feature, featurizer->featurize("tEst"));
    EXPECT_EQ(tokens[4].address, 4);
    EXPECT_EQ(tokens[4].offset, (size_t)46);
    EXPECT_EQ(tokens[4].length, (size_t)4);
    EXPECT_EQ(tokens[5].feature, featurizer->featurize("test"));
    EXPECT_EQ(tokens[5].address, 4);
    EXPECT_EQ(tokens[5].offset, (size_t)46);
    EXPECT_EQ(tokens[5].length, (size_t)4);
  }
}

TEST(AsciiTokenizer, noXMLTokenize) {
  std::shared_ptr<cottontail::Featurizer> featurizer =
      cottontail::HashingFeaturizer::make();
  std::shared_ptr<cottontail::Tokenizer> tokenizer =
      cottontail::AsciiTokenizer::make(false);
  std::vector<cottontail::Token> tokens;
  {
    char text[] = "--)(*&^%$#@!<<<<>>";
    tokens = tokenizer->tokenize(featurizer, text, sizeof(text));
    EXPECT_EQ(tokens.size(), (size_t)0);
  }
  {
    char text[] = "the cat sat on the mat";
    tokens = tokenizer->tokenize(featurizer, text, sizeof(text));
    ASSERT_EQ(tokens.size(), (size_t)6);
    EXPECT_EQ(tokens[1].feature, featurizer->featurize("cat"));
    EXPECT_EQ(tokens[1].address, 1);
    EXPECT_EQ(tokens[1].offset, (size_t)4);
    EXPECT_EQ(tokens[1].length, (size_t)3);
    EXPECT_EQ(tokens[5].feature, featurizer->featurize("mat"));
    EXPECT_EQ(tokens[5].address, 5);
    EXPECT_EQ(tokens[5].offset, (size_t)19);
    EXPECT_EQ(tokens[5].length, (size_t)3);
  }
  {
    char text[] = "^&*the THE <the><Cat>cat<sat> sat, ON the Mat";
    tokens = tokenizer->tokenize(featurizer, text, sizeof(text));
    ASSERT_EQ(tokens.size(), (size_t)12);
    EXPECT_EQ(tokens[1].feature, featurizer->featurize("THE"));
    EXPECT_EQ(tokens[1].address, 1);
    EXPECT_EQ(tokens[1].offset, (size_t)7);
    EXPECT_EQ(tokens[1].length, (size_t)3);
    EXPECT_EQ(tokens[2].feature, featurizer->featurize("the"));
    EXPECT_EQ(tokens[2].address, 1);
    EXPECT_EQ(tokens[2].offset, (size_t)7);
    EXPECT_EQ(tokens[2].length, (size_t)3);
    EXPECT_EQ(tokens[3].feature, featurizer->featurize("the"));
    EXPECT_EQ(tokens[3].address, 2);
    EXPECT_EQ(tokens[3].offset, (size_t)12);
    EXPECT_EQ(tokens[3].length, (size_t)3);
    EXPECT_EQ(tokens[4].feature, featurizer->featurize("cat"));
    EXPECT_EQ(tokens[4].address, 3);
    EXPECT_EQ(tokens[4].offset, (size_t)17);
    EXPECT_EQ(tokens[4].length, (size_t)3);
    EXPECT_EQ(tokens[11].feature, featurizer->featurize("mat"));
    EXPECT_EQ(tokens[11].address, 9);
    EXPECT_EQ(tokens[11].offset, (size_t)42);
    EXPECT_EQ(tokens[11].length, (size_t)3);
  }
  {
    char text[] =
        "the --&*((@THE******<the test=14 ><cat>cat<sat> sat on the <mat ";
    tokens = tokenizer->tokenize(featurizer, text, sizeof(text));
    ASSERT_EQ(tokens.size(), (size_t)13);
    EXPECT_EQ(tokens[1].feature, featurizer->featurize("THE"));
    EXPECT_EQ(tokens[1].address, 1);
    EXPECT_EQ(tokens[1].offset, (size_t)11);
    EXPECT_EQ(tokens[1].length, (size_t)3);
    EXPECT_EQ(tokens[2].feature, featurizer->featurize("the"));
    EXPECT_EQ(tokens[2].address, 1);
    EXPECT_EQ(tokens[2].offset, (size_t)11);
    EXPECT_EQ(tokens[2].length, (size_t)3);
    EXPECT_EQ(tokens[3].feature, featurizer->featurize("the"));
    EXPECT_EQ(tokens[3].address, 2);
    EXPECT_EQ(tokens[3].offset, (size_t)21);
    EXPECT_EQ(tokens[3].length, (size_t)3);
    EXPECT_EQ(tokens[5].feature, featurizer->featurize("14"));
    EXPECT_EQ(tokens[5].address, 4);
    EXPECT_EQ(tokens[5].offset, (size_t)30);
    EXPECT_EQ(tokens[5].length, (size_t)2);
    EXPECT_EQ(tokens[12].feature, featurizer->featurize("mat"));
    EXPECT_EQ(tokens[12].address, 11);
    EXPECT_EQ(tokens[12].offset, (size_t)60);
    EXPECT_EQ(tokens[12].length, (size_t)3);
  }
  {
    char text[] = "    <hello djslslsjf> <world slslsl> <tag=not closed   ";
    tokens = tokenizer->tokenize(featurizer, text, sizeof(text));
    ASSERT_EQ(tokens.size(), (size_t)7);
    EXPECT_EQ(tokens[0].feature, featurizer->featurize("hello"));
    EXPECT_EQ(tokens[0].address, 0);
    EXPECT_EQ(tokens[0].offset, (size_t)5);
    EXPECT_EQ(tokens[0].length, (size_t)5);
    EXPECT_EQ(tokens[4].feature, featurizer->featurize("tag"));
    EXPECT_EQ(tokens[4].address, 4);
    EXPECT_EQ(tokens[4].offset, (size_t)38);
    EXPECT_EQ(tokens[4].length, (size_t)3);
    EXPECT_EQ(tokens[6].feature, featurizer->featurize("closed"));
    EXPECT_EQ(tokens[6].address, 6);
    EXPECT_EQ(tokens[6].offset, (size_t)46);
    EXPECT_EQ(tokens[6].length, (size_t)6);
  }
  {
    char text[] = "  +hello &ignore; &do not ignore ; &&&ignore; tEst=== ";
    tokens = tokenizer->tokenize(featurizer, text, sizeof(text));
    ASSERT_EQ(tokens.size(), (size_t)8);
    EXPECT_EQ(tokens[0].feature, featurizer->featurize("hello"));
    EXPECT_EQ(tokens[0].address, 0);
    EXPECT_EQ(tokens[0].offset, (size_t)3);
    EXPECT_EQ(tokens[0].length, (size_t)5);
    EXPECT_EQ(tokens[1].feature, featurizer->featurize("ignore"));
    EXPECT_EQ(tokens[1].address, 1);
    EXPECT_EQ(tokens[1].offset, (size_t)10);
    EXPECT_EQ(tokens[1].length, (size_t)6);
    EXPECT_EQ(tokens[4].feature, featurizer->featurize("ignore"));
    EXPECT_EQ(tokens[4].address, 4);
    EXPECT_EQ(tokens[4].offset, (size_t)26);
    EXPECT_EQ(tokens[4].length, (size_t)6);
    EXPECT_EQ(tokens[6].feature, featurizer->featurize("tEst"));
    EXPECT_EQ(tokens[6].address, 6);
    EXPECT_EQ(tokens[6].offset, (size_t)46);
    EXPECT_EQ(tokens[6].length, (size_t)4);
    EXPECT_EQ(tokens[7].feature, featurizer->featurize("test"));
    EXPECT_EQ(tokens[7].address, 6);
    EXPECT_EQ(tokens[7].offset, (size_t)46);
    EXPECT_EQ(tokens[7].length, (size_t)4);
  }
}

TEST(AsciiTokenizer, XMLSkip) {
  std::shared_ptr<cottontail::Featurizer> featurizer =
      cottontail::HashingFeaturizer::make();
  std::shared_ptr<cottontail::Tokenizer> tokenizer =
      cottontail::AsciiTokenizer::make(true);
  std::vector<cottontail::Token> tokens;
  {
    std::string text = "--)(*&^%$#@!<<<<>>";
    EXPECT_EQ(tokenizer->skip(text, 100), std::string(""));
  }
  {
    std::string text = "the cat sat on the mat";
    EXPECT_EQ(tokenizer->skip(text, 0), std::string("the cat sat on the mat"));
    EXPECT_EQ(tokenizer->skip(text, 2), std::string("sat on the mat"));
    EXPECT_EQ(tokenizer->skip(text, 6), std::string(""));
  }
  {
    std::string text = "^&*the THE <the><Cat>cat<sat> sat, ON the Mat";
    EXPECT_EQ(tokenizer->skip(text, 0),
              std::string("the THE <the><Cat>cat<sat> sat, ON the Mat"));
    EXPECT_EQ(tokenizer->skip(text, 2),
              std::string("<the><Cat>cat<sat> sat, ON the Mat"));
    EXPECT_EQ(tokenizer->skip(text, 6), std::string("sat, ON the Mat"));
  }
  {
    std::string text =
        "the --&*((@THE******<the test=14 ><cat>cat<sat> sat on the <mat ";
    EXPECT_EQ(tokenizer->skip(text, 0),
              std::string("the --&*((@THE******<the test=14 ><cat>cat<sat> sat "
                          "on the <mat "));
    EXPECT_EQ(tokenizer->skip(text, 2),
              std::string("<the test=14 ><cat>cat<sat> sat on the <mat "));
    EXPECT_EQ(tokenizer->skip(text, 6), std::string("sat on the <mat "));
  }
  {
    std::string text =
        "    <hello djslslsjf> <world slslsl> <tag=not closed   ";
    EXPECT_EQ(
        tokenizer->skip(text, 0),
        std::string("<hello djslslsjf> <world slslsl> <tag=not closed   "));
    EXPECT_EQ(tokenizer->skip(text, 2), std::string("tag=not closed   "));
    EXPECT_EQ(tokenizer->skip(text, 6), std::string(""));
  }
  {
    std::string text = "  +hello &ignore; &do not ignore ; &&&ignore; tEst=== ";
    EXPECT_EQ(
        tokenizer->skip(text, 0),
        std::string("hello &ignore; &do not ignore ; &&&ignore; tEst=== "));
    EXPECT_EQ(tokenizer->skip(text, 2),
              std::string("not ignore ; &&&ignore; tEst=== "));
    EXPECT_EQ(tokenizer->skip(text, 6), std::string(""));
  }
}

TEST(AsciiTokenizer, NoXMLSkip) {
  std::shared_ptr<cottontail::Featurizer> featurizer =
      cottontail::HashingFeaturizer::make();
  std::shared_ptr<cottontail::Tokenizer> tokenizer =
      cottontail::AsciiTokenizer::make(false);
  std::vector<cottontail::Token> tokens;
  {
    std::string text = "--)(*&^%$#@!<<<<>>";
    EXPECT_EQ(tokenizer->skip(text, 100), std::string(""));
  }
  {
    std::string text = "the cat sat on the mat";
    EXPECT_EQ(tokenizer->skip(text, 0), std::string("the cat sat on the mat"));
    EXPECT_EQ(tokenizer->skip(text, 2), std::string("sat on the mat"));
    EXPECT_EQ(tokenizer->skip(text, 6), std::string(""));
  }
  {
    std::string text = "^&*the THE <the><Cat>cat<sat> sat, ON the Mat";
    EXPECT_EQ(tokenizer->skip(text, 0),
              std::string("the THE <the><Cat>cat<sat> sat, ON the Mat"));
    EXPECT_EQ(tokenizer->skip(text, 2),
              std::string("the><Cat>cat<sat> sat, ON the Mat"));
    EXPECT_EQ(tokenizer->skip(text, 6), std::string("sat, ON the Mat"));
  }
  {
    std::string text =
        "the --&*((@THE******<the test=14 ><cat>cat<sat> sat on the <mat ";
    EXPECT_EQ(tokenizer->skip(text, 0),
              std::string("the --&*((@THE******<the test=14 ><cat>cat<sat> sat "
                          "on the <mat "));
    EXPECT_EQ(tokenizer->skip(text, 2),
              std::string("the test=14 ><cat>cat<sat> sat on the <mat "));
    EXPECT_EQ(tokenizer->skip(text, 6),
              std::string("cat<sat> sat on the <mat "));
  }
  {
    std::string text =
        "    <hello djslslsjf> <world slslsl> <tag=not closed   ";
    EXPECT_EQ(
        tokenizer->skip(text, 0),
        std::string("hello djslslsjf> <world slslsl> <tag=not closed   "));
    EXPECT_EQ(tokenizer->skip(text, 2),
              std::string("world slslsl> <tag=not closed   "));
    EXPECT_EQ(tokenizer->skip(text, 6), std::string("closed   "));
  }

  {
    std::string text = "  +hello &ignore; &do not ignore ; &&&ignore; tEst=== ";
    EXPECT_EQ(
        tokenizer->skip(text, 0),
        std::string("hello &ignore; &do not ignore ; &&&ignore; tEst=== "));
    EXPECT_EQ(tokenizer->skip(text, 2),
              std::string("do not ignore ; &&&ignore; tEst=== "));
    EXPECT_EQ(tokenizer->skip(text, 6), std::string("tEst=== "));
  }
}

TEST(AsciiTokenizer, XMLSplit) {
  std::shared_ptr<cottontail::Featurizer> featurizer =
      cottontail::HashingFeaturizer::make();
  std::shared_ptr<cottontail::Tokenizer> tokenizer =
      cottontail::AsciiTokenizer::make(true);
  std::vector<cottontail::Token> tokens;
  {
    std::string text = "--)(*&^%$#@!<<<<>>";
    std::vector<std::string> terms = tokenizer->split(text);
    EXPECT_EQ(terms.size(), (size_t)0);
  }
  {
    std::string text = "the cat sat on the mat";
    std::vector<std::string> terms = tokenizer->split(text);
    ASSERT_EQ(terms.size(), (size_t)6);
    EXPECT_EQ(terms[1], std::string("cat"));
    EXPECT_EQ(terms[2], std::string("sat"));
    EXPECT_EQ(terms[5], std::string("mat"));
  }
  {
    std::string text = "^&*the THE <the><Cat>cat<sat> sat, ON the Mat";
    std::vector<std::string> terms = tokenizer->split(text);
    ASSERT_EQ(terms.size(), (size_t)10);
    EXPECT_EQ(terms[1], std::string("THE"));
    EXPECT_EQ(terms[2], std::string("<the>"));
    EXPECT_EQ(terms[8], std::string("the"));
  }
  {
    std::string text =
        "the --&*((@THE******<the test=14 ><cat>cat<sat> sat on the <mat ";
    std::vector<std::string> terms = tokenizer->split(text);
    ASSERT_EQ(terms.size(), (size_t)10);
    EXPECT_EQ(terms[1], std::string("THE"));
    EXPECT_EQ(terms[2], std::string("<the>"));
    EXPECT_EQ(terms[8], std::string("the"));
  }
  {
    std::string text =
        "    <hello djslslsjf> <world slslsl> <tag=not closed   ";
    std::vector<std::string> terms = tokenizer->split(text);
    ASSERT_EQ(terms.size(), (size_t)5);
    EXPECT_EQ(terms[1], std::string("<world>"));
    EXPECT_EQ(terms[2], std::string("tag"));
    EXPECT_EQ(terms[4], std::string("closed"));
  }
  {
    std::string text = "  +hello &ignore; &do not ignore ; &&&ignore; tEst=== ";
    std::vector<std::string> terms = tokenizer->split(text);
    ASSERT_EQ(terms.size(), (size_t)5);
    EXPECT_EQ(terms[0], std::string("hello"));
    EXPECT_EQ(terms[1], std::string("do"));
    EXPECT_EQ(terms[4], std::string("tEst"));
  }
}

TEST(AsciiTokenizer, NoXMLSplit) {
  std::shared_ptr<cottontail::Featurizer> featurizer =
      cottontail::HashingFeaturizer::make();
  std::shared_ptr<cottontail::Tokenizer> tokenizer =
      cottontail::AsciiTokenizer::make(false);
  std::vector<cottontail::Token> tokens;
  {
    std::string text = "--)(*&^%$#@!<<<<>>";
    std::vector<std::string> terms = tokenizer->split(text);
    EXPECT_EQ(terms.size(), (size_t)0);
  }
  {
    std::string text = "the cat sat on the mat";
    std::vector<std::string> terms = tokenizer->split(text);
    ASSERT_EQ(terms.size(), (size_t)6);
    EXPECT_EQ(terms[1], std::string("cat"));
    EXPECT_EQ(terms[2], std::string("sat"));
    EXPECT_EQ(terms[5], std::string("mat"));
  }
  {
    std::string text = "^&*the THE <the><Cat>cat<sat> sat, ON the Mat";
    std::vector<std::string> terms = tokenizer->split(text);
    ASSERT_EQ(terms.size(), (size_t)10);
    EXPECT_EQ(terms[1], std::string("THE"));
    EXPECT_EQ(terms[2], std::string("the"));
    EXPECT_EQ(terms[8], std::string("the"));
  }
  {
    std::string text =
        "the --&*((@THE******<the test=14 ><cat>cat<sat> sat on the <mat ";
    std::vector<std::string> terms = tokenizer->split(text);
    ASSERT_EQ(terms.size(), (size_t)12);
    EXPECT_EQ(terms[1], std::string("THE"));
    EXPECT_EQ(terms[4], std::string("14"));
    EXPECT_EQ(terms[11], std::string("mat"));
  }
  {
    std::string text =
        "    <hello djslslsjf> <world slslsl> <tag=not closed   ";
    std::vector<std::string> terms = tokenizer->split(text);
    ASSERT_EQ(terms.size(), (size_t)7);
    EXPECT_EQ(terms[2], std::string("world"));
    EXPECT_EQ(terms[3], std::string("slslsl"));
    EXPECT_EQ(terms[6], std::string("closed"));
  }
  {
    std::string text = "  +hello &ignore; &do not ignore ; &&&ignore; tEst=== ";
    std::vector<std::string> terms = tokenizer->split(text);
    ASSERT_EQ(terms.size(), (size_t)7);
    EXPECT_EQ(terms[1], std::string("ignore"));
    EXPECT_EQ(terms[2], std::string("do"));
    EXPECT_EQ(terms[5], std::string("ignore"));
  }
}
