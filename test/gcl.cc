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
  EXPECT_EQ(txt->translate(14, 10000), "</DOC>\n");
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

TEST(GCLTest, Link) {
  cottontail::addr n = 763;
  std::string burrow = cottontail::DEFAULT_BURROW;
  std::string error;
  cottontail::addr p, q, v;
  {
    std::shared_ptr<cottontail::Working> working =
        cottontail::Working::mkdir(burrow);
    ASSERT_NE(working, nullptr);
    std::string options = "";
    std::shared_ptr<cottontail::Builder> builder =
        cottontail::SimpleBuilder::make(working, options, &error);
    ASSERT_NE(builder, nullptr);
    std::string contents;
    for (cottontail::addr i = 0; i < n; i++) {
      contents += std::to_string(i);
      contents += " ";
    }
    ASSERT_TRUE(builder->add_text(contents, &p, &q));
    for (cottontail::addr i = 0; i < n; i++)
      ASSERT_TRUE(builder->add_annotation("sub1", i, i, i - 1));
    for (cottontail::addr i = 0; i < n; i++)
      ASSERT_TRUE(builder->add_annotation("mult3", i, i, 3 * i));
    for (cottontail::addr i = 0; i < n; i++)
      if (i % 3 == 0)
        ASSERT_TRUE(builder->add_annotation("foo", i, i, (cottontail::addr)99));
      else if (i % 3 == 1)
        ASSERT_TRUE(
            builder->add_annotation("foo", i, i, (cottontail::addr)77777));
      else
        ASSERT_TRUE(builder->add_annotation("foo", i, i, (cottontail::addr)5));
    ASSERT_TRUE(builder->finalize());
  }
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::Warren::make("simple", burrow, &error);
  warren->start();
  cottontail::addr i;
  std::unique_ptr<cottontail::Hopper> h;
  h = warren->hopper_from_gcl("(@ sub1)", &error);
  h->ohr(5, &p, &q, &v);
  ASSERT_EQ(5, p);
  ASSERT_EQ(p, q);
  ASSERT_EQ(v, 0);
  i = -1;
  for (h->tau(cottontail::minfinity + 1, &p, &q, &v); p < cottontail::maxfinity;
       h->tau(p + 1, &p, &q, &v)) {
    ASSERT_EQ(i, p);
    ASSERT_EQ(p, q);
    ASSERT_EQ(v, 0);
    i += 1;
  }
  ASSERT_EQ(i, n - 1);
  h->uat(cottontail::maxfinity - 1, &p, &q, &v);
  ASSERT_EQ(n - 2, p);
  ASSERT_EQ(p, q);
  ASSERT_EQ(v, 0);
  h->rho(cottontail::minfinity + 1, &p, &q, &v);
  ASSERT_EQ(-1, p);
  ASSERT_EQ(p, q);
  ASSERT_EQ(v, 0);
  h = warren->hopper_from_gcl("(@ mult3)", &error);
  h->rho(7, &p, &q, &v);
  ASSERT_EQ(9, p);
  ASSERT_EQ(p, q);
  ASSERT_EQ(v, 0);
  i = 0;
  for (h->tau(cottontail::minfinity + 1, &p, &q, &v); p < cottontail::maxfinity;
       h->tau(p + 1, &p, &q, &v)) {
    ASSERT_EQ(i, p);
    ASSERT_EQ(p, q);
    ASSERT_EQ(v, 0);
    i += 3;
  }
  ASSERT_EQ(i, 3 * n);
  h->ohr(5, &p, &q, &v);
  ASSERT_EQ(3, p);
  ASSERT_EQ(p, q);
  ASSERT_EQ(v, 0);
  h->uat(14, &p, &q, &v);
  ASSERT_EQ(12, p);
  ASSERT_EQ(p, q);
  ASSERT_EQ(v, 0);
  h = warren->hopper_from_gcl("(@ foo)", &error);
  h->tau(7, &p, &q, &v);
  ASSERT_EQ(99, p);
  ASSERT_EQ(p, q);
  ASSERT_EQ(v, 0);
  h = warren->hopper_from_gcl("(@ foo)", &error);
  h->uat(7, &p, &q, &v);
  ASSERT_EQ(5, p);
  ASSERT_EQ(p, q);
  ASSERT_EQ(v, 0);
  h->ohr(0, &p, &q, &v);
  ASSERT_EQ(cottontail::minfinity, p);
  ASSERT_EQ(p, q);
  ASSERT_EQ(v, 0);
  h->rho(0, &p, &q, &v);
  ASSERT_EQ(5, p);
  ASSERT_EQ(p, q);
  ASSERT_EQ(v, 0);
  h->rho(100, &p, &q, &v);
  ASSERT_EQ(77777, p);
  ASSERT_EQ(p, q);
  ASSERT_EQ(v, 0);
  warren->end();
}

std::unique_ptr<cottontail::Hopper>
merge(std::unique_ptr<cottontail::Hopper> left,
      std::unique_ptr<cottontail::Hopper> right) {
  return std::make_unique<cottontail::gcl::Merge>(std::move(left),
                                                  std::move(right));
}

TEST(GCLTest, Merge) {
  std::unique_ptr<cottontail::Hopper> h0 =
      std::make_unique<cottontail::SingletonHopper>(0, 10, 0.0);
  std::unique_ptr<cottontail::Hopper> h1 =
      std::make_unique<cottontail::SingletonHopper>(1, 11, 1.0);
  std::unique_ptr<cottontail::Hopper> h2 =
      std::make_unique<cottontail::SingletonHopper>(2, 12, 2.0);
  std::unique_ptr<cottontail::Hopper> h3 =
      std::make_unique<cottontail::SingletonHopper>(3, 13, 3.0);
  std::unique_ptr<cottontail::Hopper> h4 =
      std::make_unique<cottontail::SingletonHopper>(4, 14, 4.0);
  std::unique_ptr<cottontail::Hopper> ha =
      std::make_unique<cottontail::SingletonHopper>(2, 20, -1.0);
  std::unique_ptr<cottontail::Hopper> hb =
      std::make_unique<cottontail::SingletonHopper>(3, 40, -2.0);
  std::unique_ptr<cottontail::Hopper> hz =
      std::make_unique<cottontail::SingletonHopper>(1, 11, -99.0);
  std::unique_ptr<cottontail::Hopper> hopper = merge(
      std::move(h1),
      merge(std::move(h3),
            merge(std::move(ha),
                  merge(std::move(hb),
                        merge(std::move(h4),
                              merge(std::move(hz),
                                    merge(std::move(h0), std::move(h2))))))));
  ASSERT_NE(hopper, nullptr);
  cottontail::addr p, q, i = 0;
  cottontail::fval v;
  for (hopper->tau(cottontail::minfinity + 1, &p, &q, &v);
       p < cottontail::maxfinity; hopper->tau(p + 1, &p, &q, &v)) {
    EXPECT_EQ(p, i);
    EXPECT_EQ(q, i + 10);
    EXPECT_EQ(v, (cottontail::fval)i);
    i++;
  }
  i = 0;
  for (hopper->rho(cottontail::minfinity + 1, &p, &q, &v);
       q < cottontail::maxfinity; hopper->rho(q + 1, &p, &q, &v)) {
    EXPECT_EQ(p, i);
    EXPECT_EQ(q, i + 10);
    EXPECT_EQ(v, (cottontail::fval)i);
    i++;
  }
  i = 4;
  for (hopper->uat(cottontail::maxfinity - 1, &p, &q, &v);
       q > cottontail::minfinity; hopper->uat(q - 1, &p, &q, &v)) {
    EXPECT_EQ(p, i);
    EXPECT_EQ(q, i + 10);
    EXPECT_EQ(v, (cottontail::fval)i);
    --i;
  }
  i = 4;
  for (hopper->ohr(cottontail::maxfinity - 1, &p, &q, &v);
       p > cottontail::minfinity; hopper->ohr(p - 1, &p, &q, &v)) {
    EXPECT_EQ(p, i);
    EXPECT_EQ(q, i + 10);
    EXPECT_EQ(v, (cottontail::fval)i);
    --i;
  }
}
