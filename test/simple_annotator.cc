#include <fstream>
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

TEST(SimpleAnnotator, Annotating) {
  const char *genesis[] = {
      "In the beginning God created the heaven and the earth.",
      "And the earth was without form, and void; and darkness was upon the "
      "face of the deep. And the Spirit of God moved upon the face of the "
      "waters.",
      "And God said, Let there be light: and there was light.",
      "And God saw the light, that it was good: and God divided the light from "
      "the darkness.",
      "And God called the light Day, and the darkness he called Night. And the "
      "evening and the morning were the first day.",
      "And God said, Let there be a firmament in the midst of the waters, and "
      "let it divide the waters from the waters.",
      "And God made the firmament, and divided the waters which were under the "
      "firmament from the waters which were above the firmament: and it was "
      "so.",
      "And God called the firmament Heaven. And the evening and the morning "
      "were the second day.",
      "And God said, Let the waters under the heaven be gathered together unto "
      "one place, and let the dry land appear: and it was so.",
      "And God called the dry land Earth; and the gathering together of the "
      "waters called he Seas: and God saw that it was good."};
  std::string error;
  std::string burrow = cottontail::DEFAULT_BURROW;
  std::string appender_name = "simple";
  {
    std::shared_ptr<cottontail::Working> working =
        cottontail::Working::mkdir(burrow, &error);
    EXPECT_NE(working, nullptr);
    std::string options = "tokenizer:noxml txt:compressor:zlib";
    std::shared_ptr<cottontail::Builder> builder =
        cottontail::SimpleBuilder::make(working, options, &error);
    EXPECT_NE(builder, nullptr);
    for (size_t i = 0; i < sizeof(genesis) / sizeof(char *); i++) {
      std::string verse = std::string(genesis[i]);
      cottontail::addr p, q;
      builder->add_text(verse, &p, &q);
      builder->add_annotation(":verse", p, q);
    }
    EXPECT_TRUE(builder->finalize(&error));
  }
  {
    std::shared_ptr<cottontail::Warren> warren =
        cottontail::Warren::make("simple", burrow);
    warren->start();
    EXPECT_NE(warren, nullptr);
    std::string recipe;
    recipe = warren->idx()->recipe();
    std::shared_ptr<cottontail::Annotator> ann = cottontail::Annotator::make(
        "simple", recipe, &error, warren->working());
    EXPECT_NE(ann, nullptr);
    EXPECT_TRUE(ann->transaction());
    cottontail::addr p, q;
    std::unique_ptr<cottontail::Hopper> said =
        warren->hopper_from_gcl("\"and god said\"", &error);
    EXPECT_NE(said, nullptr);
    for (said->tau(0, &p, &q); p < cottontail::maxfinity;
         said->tau(p + 1, &p, &q))
      ann->annotate(warren->featurizer()->featurize(":said"), p, q);
    std::unique_ptr<cottontail::Hopper> called =
        warren->hopper_from_gcl("\"and god called\"", &error);
    EXPECT_NE(called, nullptr);
    for (called->tau(0, &p, &q); p < cottontail::maxfinity;
         called->tau(p + 1, &p, &q))
      ann->annotate(warren->featurizer()->featurize(":called"), p, q);
    EXPECT_TRUE(ann->ready());
    ann->commit();
    warren->end();
  }
  {
    std::shared_ptr<cottontail::Warren> warren =
        cottontail::Warren::make("simple", burrow);
    warren->start();
    cottontail::addr p, q;
    std::unique_ptr<cottontail::Hopper> said =
        warren->hopper_from_gcl(":said", &error);
    EXPECT_NE(said, nullptr);
    for (said->tau(0, &p, &q); p < cottontail::maxfinity;
         said->tau(p + 1, &p, &q))
      EXPECT_TRUE(warren->txt()->translate(p, q) ==
                  std::string("And God said, "));
    std::unique_ptr<cottontail::Hopper> called =
        warren->hopper_from_gcl(":called", &error);
    EXPECT_NE(called, nullptr);
    for (called->tau(0, &p, &q); p < cottontail::maxfinity;
         called->tau(p + 1, &p, &q))
      EXPECT_TRUE(warren->txt()->translate(p, q) ==
                  std::string("And God called "));
    warren->end();
  }
}

TEST(SimpleAnnotator, Multiple) {
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
    parameters["add_file_size"] = "173";
    recipe = cottontail::freeze(parameters);
  }
  {
    std::shared_ptr<cottontail::Annotator> ann =
        cottontail::Annotator::make("simple", recipe, nullptr, working);
    EXPECT_NE(ann, nullptr);
    EXPECT_TRUE(ann->transaction());
    for (cottontail::addr i = 1823; i < 3331; i++)
      ann->annotate(1, i, i + 73, (cottontail::addr)6);
    EXPECT_TRUE(ann->ready());
    ann->commit();
    {
      std::shared_ptr<cottontail::Idx> idx =
          cottontail::Idx::make("simple", recipe, nullptr, working);
      EXPECT_NE(idx, nullptr);
      std::unique_ptr<cottontail::Hopper> one = idx->hopper(1);
      EXPECT_NE(one, nullptr);
      cottontail::addr p = 0;
      for (cottontail::addr i = 1823; i < 3331; i++) {
        cottontail::addr q, v;
        one->tau(p + 1, &p, &q, &v);
        EXPECT_EQ(i, p);
        EXPECT_EQ(i + 73, q);
        EXPECT_EQ(6, v);
      }
    }
    EXPECT_TRUE(ann->transaction());
    for (cottontail::addr i = 1231; i < 3331; i++)
      ann->annotate(2, i, i + 97, (cottontail::addr)12);
    for (cottontail::addr i = 1231; i < 1823; i++)
      ann->annotate(1, i, i + 73, (cottontail::addr)6);
    EXPECT_TRUE(ann->ready());
    ann->commit();
    {
      std::shared_ptr<cottontail::Idx> idx =
          cottontail::Idx::make("simple", recipe, nullptr, working);
      EXPECT_NE(idx, nullptr);
      std::unique_ptr<cottontail::Hopper> one = idx->hopper(1);
      EXPECT_NE(one, nullptr);
      std::unique_ptr<cottontail::Hopper> two = idx->hopper(2);
      EXPECT_NE(two, nullptr);
      cottontail::addr p = 0;
      for (cottontail::addr i = 1231; i < 3331; i++) {
        cottontail::addr q, v;
        one->tau(p + 1, &p, &q, &v);
        EXPECT_EQ(i, p);
        EXPECT_EQ(i + 73, q);
        EXPECT_EQ(6, v);
      }
      p = 0;
      for (cottontail::addr i = 1231; i < 3331; i++) {
        cottontail::addr q, v;
        two->tau(p + 1, &p, &q, &v);
        EXPECT_EQ(i, p);
        EXPECT_EQ(i + 97, q);
        EXPECT_EQ(12, v);
      }
    }
    EXPECT_TRUE(ann->transaction());
    ann->annotate(1, 2, 3, 999.0);
    EXPECT_TRUE(ann->ready());
    ann->abort();
    EXPECT_TRUE(ann->transaction());
    EXPECT_FALSE(ann->transaction());
    ann->annotate(3, 1, 2, 999.0);
    ann->abort();
    EXPECT_TRUE(ann->transaction());
    for (cottontail::addr i = 857; i < 3331; i++)
      ann->annotate(3, i, i + 127, (cottontail::addr)18);
    for (cottontail::addr i = 857; i < 1231; i++)
      ann->annotate(2, i, i + 97, (cottontail::addr)12);
    for (cottontail::addr i = 857; i < 1231; i++)
      ann->annotate(1, i, i + 73, (cottontail::addr)6);
    EXPECT_TRUE(ann->ready());
    ann->commit();
    {
      std::shared_ptr<cottontail::Idx> idx =
          cottontail::Idx::make("simple", recipe, nullptr, working);
      EXPECT_NE(idx, nullptr);
      std::unique_ptr<cottontail::Hopper> one = idx->hopper(1);
      EXPECT_NE(one, nullptr);
      std::unique_ptr<cottontail::Hopper> two = idx->hopper(2);
      EXPECT_NE(two, nullptr);
      std::unique_ptr<cottontail::Hopper> three = idx->hopper(3);
      EXPECT_NE(three, nullptr);
      cottontail::addr p = 0;
      for (cottontail::addr i = 857; i < 3331; i++) {
        cottontail::addr q, v;
        one->tau(p + 1, &p, &q, &v);
        EXPECT_EQ(i, p);
        EXPECT_EQ(i + 73, q);
        EXPECT_EQ(6, v);
      }
      p = 0;
      for (cottontail::addr i = 857; i < 3331; i++) {
        cottontail::addr q, v;
        two->tau(p + 1, &p, &q, &v);
        EXPECT_EQ(i, p);
        EXPECT_EQ(i + 97, q);
        EXPECT_EQ(12, v);
      }
      p = 0;
      for (cottontail::addr i = 857; i < 3331; i++) {
        cottontail::addr q, v;
        three->tau(p + 1, &p, &q, &v);
        EXPECT_EQ(i, p);
        EXPECT_EQ(i + 127, q);
        EXPECT_EQ(18, v);
      }
    }
  }
}
