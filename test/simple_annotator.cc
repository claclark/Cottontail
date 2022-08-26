#include<fstream>
#include <memory>
#include <string>

#include "gtest/gtest.h"

#include "src/cottontail.h"

TEST(SimpleAnnotator, FromEmpty) {
  std::string burrow = cottontail::DEFAULT_BURROW;
  std::shared_ptr<cottontail::Working> working =
      cottontail::Working::mkdir(burrow);
  EXPECT_NE(working, nullptr);
  std::string recipe;
  {
    std::string options = "tokenizer:noxml";
    std::shared_ptr<cottontail::Builder> builder =
        cottontail::SimpleBuilder::make(working, options);
    EXPECT_NE(builder, nullptr);
    EXPECT_TRUE(builder->finalize());
    std::shared_ptr<cottontail::Warren> warren =
        cottontail::Warren::make("simple", burrow);
    warren->start();
    EXPECT_NE(warren, nullptr);
    recipe = warren->idx()->recipe();
    warren->end();
    std::map<std::string, std::string> parameters;
    EXPECT_TRUE(cottontail::cook(recipe, &parameters));
    parameters["add_file_size"] = "101";
    recipe = cottontail::freeze(parameters);
  }
  {
    std::shared_ptr<cottontail::Annotator> ann =
        cottontail::Annotator::make("simple", recipe, nullptr, working);
    EXPECT_NE(ann, nullptr);
    EXPECT_TRUE(ann->transaction());
    for (cottontail::addr i = 0; i < 7919; i++)
      ann->annotate(i, i, i, i);
    EXPECT_TRUE(ann->ready());
    ann->commit();
  }
  {
    std::shared_ptr<cottontail::Idx> idx =
        cottontail::Idx::make("simple", recipe, nullptr, working);
    EXPECT_NE(idx, nullptr);
    for (cottontail::addr i = 0; i < 7919; i++) {
      std::unique_ptr<cottontail::Hopper> hopper = idx->hopper(i);
      EXPECT_NE(hopper, nullptr);
      cottontail::addr p, q, v;
      hopper->tau(0, &p, &q, &v);
      EXPECT_EQ(i, p);
      EXPECT_EQ(i, q);
      EXPECT_EQ(i, v);
    }
  }
  {
    std::shared_ptr<cottontail::Annotator> ann =
        cottontail::Annotator::make("simple", recipe, nullptr, working);
    EXPECT_NE(ann, nullptr);
    EXPECT_TRUE(ann->transaction());
    for (cottontail::addr i = 10000; i < 20000; i++)
      ann->annotate(999, i, i + 99, 12345.00);
    EXPECT_TRUE(ann->ready());
    ann->abort();
  }
  {
    std::shared_ptr<cottontail::Annotator> ann =
        cottontail::Annotator::make("simple", recipe, nullptr, working);
    EXPECT_NE(ann, nullptr);
    EXPECT_TRUE(ann->transaction());
    for (cottontail::addr i = 10000; i < 20000; i++) {
      ann->annotate(123, i, i + 13, 123.00);
      ann->annotate(456, i, i + 17, 456.00);
    }
    {
      std::shared_ptr<cottontail::Annotator> ann2 =
          cottontail::Annotator::make("simple", recipe, nullptr, working);
      EXPECT_NE(ann2, nullptr);
      EXPECT_FALSE(ann->transaction());
      EXPECT_FALSE(ann2->transaction());
    }
    EXPECT_TRUE(ann->ready());
    ann->commit();
  }
  {
    std::shared_ptr<cottontail::Idx> idx =
        cottontail::Idx::make("simple", recipe, nullptr, working);
    EXPECT_NE(idx, nullptr);
    {
      cottontail::fval v;
      cottontail::addr i, p, q;
      std::unique_ptr<cottontail::Hopper> hopper = idx->hopper(123);
      EXPECT_NE(hopper, nullptr);
      hopper->tau(0, &p, &q, &i);
      EXPECT_EQ(p, 123);
      EXPECT_EQ(q, 123);
      EXPECT_EQ(i, 123);
      {
        std::unique_ptr<cottontail::Hopper> hopper2 = idx->hopper(999);
        EXPECT_NE(hopper2, nullptr);
        hopper2->tau(0, &p, &q, &i);
        EXPECT_EQ(p, 999);
        hopper2->tau(9999, &p, &q, &i);
        EXPECT_EQ(p, cottontail::maxfinity);
      }
      i = 10000;
      for (hopper->tau(9999, &p, &q, &v); p < cottontail::maxfinity;
           hopper->tau(p + 1, &p, &q, &v)) {
        EXPECT_EQ(i, p);
        EXPECT_EQ(i + 13, q);
        EXPECT_EQ(v, 123.00);
        i++;
      }
    }
    for (cottontail::addr i = 0; i < 7919; i++) {
      std::unique_ptr<cottontail::Hopper> hopper = idx->hopper(i);
      EXPECT_NE(hopper, nullptr);
      cottontail::addr p, q, v;
      hopper->tau(0, &p, &q, &v);
      EXPECT_EQ(i, p);
      EXPECT_EQ(i, q);
      EXPECT_EQ(i, v);
    }
    {
      cottontail::fval v;
      cottontail::addr i, p, q;
      std::unique_ptr<cottontail::Hopper> hopper = idx->hopper(456);
      EXPECT_NE(hopper, nullptr);
      hopper->tau(0, &p, &q, &i);
      EXPECT_EQ(p, 456);
      EXPECT_EQ(q, 456);
      EXPECT_EQ(i, 456);
      i = 10000;
      for (hopper->tau(p + 1, &p, &q, &v); p < cottontail::maxfinity;
           hopper->tau(p + 1, &p, &q, &v)) {
        EXPECT_EQ(i, p);
        EXPECT_EQ(i + 17, q);
        EXPECT_EQ(v, 456.00);
        i++;
      }
    }
  }
  {
    std::shared_ptr<cottontail::Annotator> ann =
        cottontail::Annotator::make("simple", recipe, nullptr, working);
    EXPECT_NE(ann, nullptr);
    EXPECT_TRUE(ann->transaction());
    for (cottontail::addr i = 10000; i < 20000; i++)
      ann->annotate(999, i, i + 99, 12345.00);
    EXPECT_TRUE(ann->ready());
  }
  cottontail::Annotator::recover("simple", recipe, false, nullptr, working);
  {
    std::shared_ptr<cottontail::Idx> idx =
        cottontail::Idx::make("simple", recipe, nullptr, working);
    EXPECT_NE(idx, nullptr);
    cottontail::addr p, q, v;
    std::unique_ptr<cottontail::Hopper> hopper = idx->hopper(999);
    EXPECT_NE(hopper, nullptr);
    hopper->tau(0, &p, &q, &v);
    EXPECT_EQ(p, 999);
    EXPECT_EQ(q, 999);
    EXPECT_EQ(v, 999);
    hopper->tau(9999, &p, &q, &v);
    EXPECT_EQ(p, cottontail::maxfinity);
    EXPECT_EQ(q, cottontail::maxfinity);
    EXPECT_EQ(v, 0);
  }
  {
    std::shared_ptr<cottontail::Annotator> ann =
        cottontail::Annotator::make("simple", recipe, nullptr, working);
    EXPECT_NE(ann, nullptr);
    EXPECT_TRUE(ann->transaction());
    for (cottontail::addr i = 10000; i < 20000; i++)
      ann->annotate(999, i, i + 99, 12345.00);
    EXPECT_TRUE(ann->ready());
  }
  cottontail::Annotator::recover("simple", recipe, true, nullptr, working);
  {
    std::shared_ptr<cottontail::Idx> idx =
        cottontail::Idx::make("simple", recipe, nullptr, working);
    EXPECT_NE(idx, nullptr);
    cottontail::fval f;
    cottontail::addr p, q, v;
    std::unique_ptr<cottontail::Hopper> hopper = idx->hopper(999);
    EXPECT_NE(hopper, nullptr);
    for (cottontail::addr i = 10000; i < 20000; i++) {
      hopper->tau(900, &p, &q, &v);
      EXPECT_EQ(p, 999);
      EXPECT_EQ(q, 999);
      EXPECT_EQ(v, 999);
      hopper->tau(i, &p, &q, &f);
      EXPECT_EQ(p, i);
      EXPECT_EQ(q, i + 99);
      EXPECT_EQ(f, 12345.00);
    }
  }
}
