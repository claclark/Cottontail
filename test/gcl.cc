#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#include "gtest/gtest.h"

#include "src/cottontail.h"

TEST(GCLTest, E2E) {
  std::string testing = "testing";
  std::shared_ptr<cottontail::Working> working =
      cottontail::Working::mkdir(testing);
  std::string error;
  std::string featurizer_name = "hashing";
  std::string featurizer_recipe = "";
  std::shared_ptr<cottontail::Featurizer> featurizer =
      cottontail::Featurizer::make(featurizer_name, featurizer_recipe, &error);
  ASSERT_NE(featurizer, nullptr) << error;
  std::string tokenizer_name = "ascii";
  std::string tokenizer_recipe = "xml";
  std::shared_ptr<cottontail::Tokenizer> tokenizer =
      cottontail::Tokenizer::make(tokenizer_name, tokenizer_recipe, &error);
  ASSERT_NE(tokenizer, nullptr) << error;
  std::shared_ptr<cottontail::Builder> builder =
      cottontail::SimpleBuilder::make(working, featurizer, tokenizer, &error);
  ASSERT_NE(builder, nullptr) << error;
  builder->verbose(false);
  std::vector<std::string> text;
  text.push_back("test/test0.txt");
  bool okay = cottontail::build_trec(text, builder, &error);
  ASSERT_TRUE(okay) << error;
  std::string simple = "simple";
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::Warren::make(simple, testing, &error);
  ASSERT_NE(warren, nullptr) << error;
  warren->start();
  featurizer = warren->featurizer();
  tokenizer = warren->tokenizer();
  std::shared_ptr<cottontail::Txt> txt = warren->txt();
  std::shared_ptr<cottontail::Idx> idx = warren->idx();

  auto compile = [&](std::string g) -> std::unique_ptr<cottontail::Hopper> {
    std::shared_ptr<cottontail::gcl::SExpression> expr =
        cottontail::gcl::SExpression::from_string(g, &error);
    EXPECT_NE(expr, nullptr) << error;
    if (expr == nullptr)
      return nullptr;
    std::unique_ptr<cottontail::Hopper> fluffy =
        expr->to_hopper(featurizer, idx);
    EXPECT_NE(fluffy, nullptr) << error;
    if (fluffy == nullptr)
      return nullptr;
    return fluffy;
  };

  EXPECT_EQ(txt->translate(-100, -10), "");
  EXPECT_EQ(txt->translate(11, 10), "");
  EXPECT_EQ(txt->translate(0, 1), "<DOC>\n<DOCNO> ");
  EXPECT_EQ(txt->translate(-1000, 1), "<DOC>\n<DOCNO> ");
  EXPECT_EQ(txt->translate(8, 10), "Hello world hello ");
  EXPECT_EQ(txt->translate(14, 10000), "</DOC>\n\n");
  EXPECT_EQ(txt->translate(100, 10000), "");

  std::unique_ptr<cottontail::Hopper> h0 = compile("hello");
  ASSERT_NE(h0, nullptr);
  cottontail::addr p, q;
  h0->tau(cottontail::minfinity + 1, &p, &q);
  EXPECT_EQ(p, 5);
  EXPECT_EQ(q, 5);
  h0->tau(6, &p, &q);
  EXPECT_EQ(p, 8);
  EXPECT_EQ(q, 8);
  h0->rho(9, &p, &q);
  EXPECT_EQ(p, 10);
  EXPECT_EQ(q, 10);
  h0->rho(11, &p, &q);
  EXPECT_EQ(p, 13);
  EXPECT_EQ(q, 13);
  h0->uat(cottontail::maxfinity - 1, &p, &q);
  EXPECT_EQ(p, 13);
  EXPECT_EQ(q, 13);
  h0->uat(7, &p, &q);
  EXPECT_EQ(p, 5);
  EXPECT_EQ(q, 5);
  h0->ohr(12, &p, &q);
  EXPECT_EQ(p, 10);
  EXPECT_EQ(q, 10);
  h0->ohr(10, &p, &q);
  EXPECT_EQ(p, 10);
  EXPECT_EQ(q, 10);
  h0->tau(cottontail::minfinity, &p, &q);
  EXPECT_EQ(p, cottontail::minfinity);
  EXPECT_EQ(q, cottontail::minfinity);
  h0->tau(cottontail::maxfinity, &p, &q);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);

  std::unique_ptr<cottontail::Hopper> h1 = compile("NOTATTOKEN");
  ASSERT_NE(h1, nullptr);
  h1->tau(cottontail::minfinity + 1, &p, &q);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  h1->uat(cottontail::maxfinity - 1, &p, &q);
  EXPECT_EQ(p, cottontail::minfinity);
  EXPECT_EQ(q, cottontail::minfinity);

  std::unique_ptr<cottontail::Hopper> h2 = compile("(... hello world)");
  ASSERT_NE(h2, nullptr);
  h2->tau(cottontail::minfinity + 1, &p, &q);
  EXPECT_EQ(p, 5);
  EXPECT_EQ(q, 6);
  h2->uat(cottontail::maxfinity - 1, &p, &q);
  EXPECT_EQ(p, 10);
  EXPECT_EQ(q, 11);
  h2->rho(10, &p, &q);
  EXPECT_EQ(p, 10);
  EXPECT_EQ(q, 11);

  std::unique_ptr<cottontail::Hopper> h3 = compile("(^ hello world)");
  ASSERT_NE(h3, nullptr);
  h3->tau(cottontail::minfinity + 1, &p, &q);
  EXPECT_EQ(p, 5);
  EXPECT_EQ(q, 6);
  h3->tau(12, &p, &q);
  EXPECT_EQ(p, 12);
  EXPECT_EQ(q, 13);
  h3->uat(cottontail::maxfinity - 1, &p, &q);
  EXPECT_EQ(p, 12);
  EXPECT_EQ(q, 13);
  h3->rho(10, &p, &q);
  EXPECT_EQ(p, 9);
  EXPECT_EQ(q, 10);
  h3->ohr(10, &p, &q);
  EXPECT_EQ(p, 10);
  EXPECT_EQ(q, 11);

  std::unique_ptr<cottontail::Hopper> h4 =
      compile("(>> (... <TITLE> </TITLE>) (^ hello world))");
  ASSERT_NE(h4, nullptr);
  h4->ohr(cottontail::maxfinity - 1, &p, &q);
  EXPECT_EQ(p, 4);
  EXPECT_EQ(q, 7);
}
