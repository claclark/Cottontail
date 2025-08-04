#include <cassert>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

#include "gtest/gtest.h"

#include "src/array_hopper.h"
#include "src/core.h"
#include "src/featurizer.h"
#include "src/hopper.h"
#include "src/idx.h"
#include "src/simple_annotator.h"
#include "src/simple_builder.h"
#include "src/simple_idx.h"
#include "src/simple_posting.h"
#include "src/simple_txt.h"
#include "src/tokenizer.h"
#include "src/txt.h"
#include "src/warren.h"
#include "src/working.h"

#define ASSERT_ERROR_EQ(expected_lit, error_str)                               \
  ASSERT_EQ((error_str).substr(0, sizeof(expected_lit) - 1), expected_lit)

TEST(Simple, BuilderOptions) {
  std::string error;
  std::string test0 = "idx:fvalue_compressor:null tokenizer:noxml";
  EXPECT_TRUE(cottontail::SimpleBuilder::check(test0, &error));
  ASSERT_ERROR_EQ("", error);
  std::string burrow_name = "the.burrow";
  auto working = cottontail::Working::mkdir(burrow_name, &error);
  ASSERT_NE(working, nullptr);
  auto builder = cottontail::SimpleBuilder::make(working, test0, &error);
  ASSERT_NE(builder, nullptr);
  std::string test1 = "idx:fvalue_compressor:xxx tokenizer:noxml";
  EXPECT_FALSE(cottontail::SimpleBuilder::check(test1, &error));
  ASSERT_ERROR_EQ("No Compressor named: xxx", error);
  std::string test2 = "idx:fvalue_compressor:xxx foo:noxml";
  EXPECT_FALSE(cottontail::SimpleBuilder::check(test2, &error));
  ASSERT_ERROR_EQ("Option not found: foo", error);
}

// End-to-end testing of simple index structures

TEST(Simple, E2E) {
  std::string error;
  std::string empty_recipe = "";
  std::string bad_recipe = "bad recipe";
  EXPECT_TRUE(cottontail::SimpleTxt::check(empty_recipe));
  EXPECT_FALSE(cottontail::SimpleTxt::check(bad_recipe));
  EXPECT_TRUE(cottontail::SimpleIdx::check(empty_recipe));
  EXPECT_FALSE(cottontail::SimpleIdx::check(bad_recipe));
  std::string burrow_name = "test.burrow";
  std::string featurizer_name = "hashing";
  std::string tokenizer_name = "ascii";
  std::string simple = "simple";
  std::string hello_tag = ":hello";
  std::string dog_tag = ":dog";
  std::string cat_tag = ":cat";
  std::string junk_tag = ":junk";
  std::shared_ptr<cottontail::Working> working =
      cottontail::Working::mkdir(burrow_name, &error);
  ASSERT_NE(working, nullptr);
  std::shared_ptr<cottontail::Featurizer> featurizer =
      cottontail::Featurizer::make(featurizer_name, empty_recipe, &error);
  ASSERT_NE(featurizer, nullptr);
  std::shared_ptr<cottontail::Tokenizer> tokenizer =
      cottontail::Tokenizer::make(tokenizer_name, empty_recipe, &error);
  ASSERT_NE(tokenizer, nullptr);
  cottontail::addr NUMBER = 100000;
  cottontail::addr TOKENS = 18;
  cottontail::addr CHARACTERS = 96;
  {
    std::shared_ptr<cottontail::Builder> builder =
        cottontail::SimpleBuilder::make(working, featurizer, tokenizer, &error,
                                        128 * 1024);
    ASSERT_NE(builder, nullptr);
    std::string text0 = "Hello world!!!";
    std::string text1 = "...The quick brown fox jumped over the lazy dog...";
    std::string text2 = "The cat in the hat came back?";
    for (cottontail::addr i = 0; i < NUMBER; i++) {
      cottontail::addr p, q;
      EXPECT_TRUE(builder->add_text(text0, &p, &q, &error));
      EXPECT_TRUE(builder->add_annotation(hello_tag, p, q, i, &error));
      EXPECT_EQ(p, i * TOKENS);
      EXPECT_EQ(q, p + 1);
      EXPECT_TRUE(builder->add_text(text1, &p, &q, &error));
      EXPECT_TRUE(builder->add_annotation(dog_tag, p, q, 0.0, &error));
      EXPECT_EQ(p, i * TOKENS + 2);
      EXPECT_EQ(q, p + 8);
      EXPECT_TRUE(builder->add_text(text2, &p, &q, &error));
      EXPECT_TRUE(builder->add_annotation(cat_tag, p, q, 0.0, &error));
      EXPECT_EQ(p, i * TOKENS + 11);
      EXPECT_EQ(q, p + 6);
    }
    builder->finalize(&error);
  }
  {
    std::shared_ptr<cottontail::Txt> txt =
        cottontail::Txt::make(simple, empty_recipe, &error, tokenizer, working);
    ASSERT_NE(txt, nullptr);
    txt = txt->clone();
    ASSERT_NE(txt, nullptr);
    std::string everything =
        txt->translate(cottontail::minfinity, cottontail::maxfinity);
    EXPECT_EQ(everything.size(), (size_t)(CHARACTERS * NUMBER));
    EXPECT_EQ(everything.substr(18, 3), "The");
    EXPECT_EQ(txt->translate(-10, -8), "");
    EXPECT_EQ(txt->translate(-10, 0), "Hello ");
    EXPECT_EQ(txt->translate(10, 8), "");
    EXPECT_EQ(txt->translate(10000000, 10000010), "");
    EXPECT_EQ(txt->tokens(), TOKENS * NUMBER);
    EXPECT_EQ(txt->tokens(), TOKENS * NUMBER);
    EXPECT_EQ(txt->translate(-10, -8), "");
    for (cottontail::addr i = 0; i < NUMBER; i++) {
      cottontail::addr p = TOKENS * i + 1;
      cottontail::addr q = TOKENS * i + 11;
      std::string expected_text =
          "world!!!\n...The quick brown fox jumped over the lazy dog...\nThe ";
      std::string actual_text = txt->translate(p, q);
      EXPECT_EQ(expected_text, actual_text);
    }
    for (cottontail::addr i = 0; i < NUMBER; i++) {
      cottontail::addr p = TOKENS * i + 15;
      cottontail::addr q = TOKENS * i + 17;
      std::string expected_text = "hat came back?\n";
      std::string actual_text = txt->translate(p, q);
      EXPECT_EQ(expected_text, actual_text);
    }
  }
  {
    std::shared_ptr<cottontail::Idx> idx =
        cottontail::Idx::make(simple, empty_recipe, &error, working);
    std::string fox = "fox";
    ASSERT_EQ(idx->count(featurizer->featurize(fox)), NUMBER);
    std::unique_ptr<cottontail::Hopper> hopper;
    ASSERT_NE((hopper = idx->hopper(featurizer->featurize(fox))), nullptr);
    cottontail::addr k, p, q, expected = 5;
    cottontail::fval v;
    for (p = cottontail::minfinity; p < cottontail::maxfinity;) {
      hopper->tau(p + 1, &p, &q, &v);
      EXPECT_EQ(p, q);
      EXPECT_EQ(v, 0.0);
      if (p < cottontail::maxfinity) {
        EXPECT_EQ(p, expected);
      }
      expected += TOKENS;
    }
    EXPECT_EQ(expected, TOKENS * (NUMBER + 1) + 5);
    ASSERT_EQ(idx->count(featurizer->featurize(fox)), NUMBER);
    for (int i = 0; i < 10; i++) {
      std::string the = "the";
      ASSERT_EQ(idx->count(featurizer->featurize(the)), 4 * NUMBER);
      hopper = idx->hopper(featurizer->featurize(the));
      ASSERT_NE(hopper, nullptr);
      hopper->ohr(cottontail::maxfinity - 1, &p, &q);
      EXPECT_EQ(p, 1799996);
      hopper->ohr(p - 1, &p, &q);
      EXPECT_EQ(p, 1799993);
      hopper->ohr(p - 1, &p, &q);
      EXPECT_EQ(p, 1799990);
      hopper->ohr(p - 1, &p, &q);
      EXPECT_EQ(p, 1799984);
      hopper->ohr(p - 1, &p, &q);
      EXPECT_EQ(p, 1799978);
      hopper->ohr(p - 1, &p, &q);
      EXPECT_EQ(p, 1799975);
    }
    ASSERT_EQ(idx->count(featurizer->featurize(hello_tag)), NUMBER);
    ASSERT_NE((hopper = idx->hopper(featurizer->featurize(hello_tag))),
              nullptr);
    expected = 0;
    cottontail::addr expected_value = 0, actual_value;
    for (k = p = cottontail::minfinity + 1; p < cottontail::maxfinity;
         k = p + 1) {
      hopper->tau(k, &p, &q, &actual_value);
      if (p < cottontail::maxfinity) {
        EXPECT_EQ(p + 1, q);
        EXPECT_EQ(actual_value, expected_value);
        EXPECT_EQ(p, expected);
      } else {
        EXPECT_EQ(q, cottontail::maxfinity);
        EXPECT_EQ(v, 0.0);
      }
      expected += TOKENS;
      expected_value++;
    }
    ASSERT_EQ(idx->count(featurizer->featurize(hello_tag)), NUMBER);
    ASSERT_EQ(idx->count(featurizer->featurize(cat_tag)), NUMBER);
    EXPECT_EQ(expected, TOKENS * (NUMBER + 1));
    EXPECT_EQ(expected_value, NUMBER + 1.0);
    ASSERT_NE((hopper = idx->hopper(featurizer->featurize(cat_tag))), nullptr);
    expected = 11;
    for (k = p = cottontail::minfinity + 1; p < cottontail::maxfinity;
         k = p + 1) {
      hopper->tau(k, &p, &q, &v);
      EXPECT_EQ(v, 0.0);
      if (p < cottontail::maxfinity) {
        EXPECT_EQ(p + 6, q);
        EXPECT_EQ(p, expected);
      } else {
        EXPECT_EQ(q, cottontail::maxfinity);
      }
      expected += TOKENS;
    }
    EXPECT_EQ(expected, TOKENS * (NUMBER + 1) + 11);
    ASSERT_EQ(idx->count(featurizer->featurize(cat_tag)), NUMBER);
    std::string ann_filename = working->make_temp();
    {
      cottontail::addr junk = featurizer->featurize(junk_tag);
      cottontail::addr cat = featurizer->featurize(cat_tag);
      std::fstream annf(ann_filename, std::ios::binary | std::ios::out);
      cottontail::Annotation annr0 = {junk, 36, 64, 1.0};
      annf.write(reinterpret_cast<char *>(&annr0), sizeof(annr0));
      cottontail::Annotation annr1 = {junk, 64, 72, 2.0};
      annf.write(reinterpret_cast<char *>(&annr1), sizeof(annr1));
      cottontail::Annotation annr2 = {cat, 1003, 1009, 2.0};
      annf.write(reinterpret_cast<char *>(&annr2), sizeof(annr2));
      cottontail::Annotation annr3 = {cat, 2000, 2014, 2.0};
      annf.write(reinterpret_cast<char *>(&annr3), sizeof(annr3));
      cottontail::Annotation annr4 = {junk, 12, 32, 3.0};
      annf.write(reinterpret_cast<char *>(&annr4), sizeof(annr4));
      cottontail::Annotation annr5 = {junk, 24, 48, 4.0};
      annf.write(reinterpret_cast<char *>(&annr5), sizeof(annr5));
      std::string lazy = "lazy";
      cottontail::Annotation annr6 = {featurizer->featurize(lazy), 2012, 2016,
                                      99.0};
      annf.write(reinterpret_cast<char *>(&annr6), sizeof(annr6));
    }
    std::string sorted_ann_filename;
    ASSERT_TRUE(cottontail::sort_annotations(working, ann_filename,
                                             &sorted_ann_filename, &error));
    ASSERT_TRUE(cottontail::check_annotations(sorted_ann_filename, &error));
    {
      std::shared_ptr<cottontail::Annotator> ann =
          cottontail::SimpleAnnotator::make(idx->recipe(), working, &error);
      ASSERT_NE(ann, nullptr);
      ASSERT_TRUE(ann->transaction());
      ASSERT_TRUE(ann->annotate(sorted_ann_filename, &error));
      ASSERT_TRUE(ann->ready());
      ann->commit();
    }
    std::remove(sorted_ann_filename.c_str());
  }
  {
    std::shared_ptr<cottontail::Idx> idx =
        cottontail::Idx::make(simple, empty_recipe, &error, working);
    std::unique_ptr<cottontail::Hopper> hopper;
    ASSERT_NE((hopper = idx->hopper(featurizer->featurize(junk_tag))), nullptr);
    cottontail::addr p, q;
    cottontail::fval v;
    hopper->tau(60, &p, &q, &v);
    EXPECT_EQ(p, 64);
    EXPECT_EQ(q, 72);
    EXPECT_EQ(v, 2.0);
    hopper->rho(60, &p, &q, &v);
    EXPECT_EQ(p, 36);
    EXPECT_EQ(q, 64);
    EXPECT_EQ(v, 1.0);
    hopper->uat(60, &p, &q, &v);
    EXPECT_EQ(p, 24);
    EXPECT_EQ(q, 48);
    EXPECT_EQ(v, 4.0);
    hopper->ohr(15, &p, &q, &v);
    EXPECT_EQ(p, 12);
    EXPECT_EQ(q, 32);
    EXPECT_EQ(v, 3.0);
    ASSERT_NE((hopper = idx->hopper(featurizer->featurize(cat_tag))), nullptr);
    hopper->tau(1002, &p, &q, &v);
    EXPECT_EQ(p, 1003);
    EXPECT_EQ(q, 1009);
    EXPECT_EQ(v, 2.0);
    hopper->tau(1999, &p, &q, &v);
    EXPECT_EQ(p, 2000);
    EXPECT_EQ(q, 2014);
    EXPECT_EQ(v, 2.0);
    std::string lazy = "lazy";
    ASSERT_NE((hopper = idx->hopper(featurizer->featurize(lazy))), nullptr);
    hopper->tau(2000, &p, &q, &v);
    EXPECT_EQ(p, 2007);
    EXPECT_EQ(q, 2007);
    EXPECT_EQ(v, 0.0);
    hopper->tau(2008, &p, &q, &v);
    EXPECT_EQ(p, 2012);
    EXPECT_EQ(q, 2016);
    EXPECT_EQ(v, 99.0);
    hopper->tau(2020, &p, &q, &v);
    EXPECT_EQ(p, 2025);
    EXPECT_EQ(q, 2025);
    EXPECT_EQ(v, 0.0);
  }
  {
    std::string error;
    std::string simple = "simple";
    std::shared_ptr<cottontail::Warren> warren;
    warren = cottontail::Warren::make(simple, burrow_name, &error);
    ASSERT_NE(warren, nullptr);
    warren->start();
    std::string key0 = "key0";
    std::string value0 = "hello";
    EXPECT_TRUE(warren->set_parameter(key0, value0, &error));
    std::string test;
    EXPECT_TRUE(warren->get_parameter(key0, &test, &error));
    EXPECT_EQ(test, value0);
    std::string key1 = "key1";
    std::string value1 = "world";
    EXPECT_TRUE(warren->set_parameter(key1, value1, &error));
    EXPECT_TRUE(warren->get_parameter(key1, &test, &error));
    EXPECT_EQ(test, value1);
    std::string foo = "foo";
    EXPECT_TRUE(warren->get_parameter(foo, &test, &error));
    EXPECT_EQ(test, "");
    EXPECT_TRUE(warren->get_parameter(key0, &test, &error));
    EXPECT_EQ(test, value0);
    warren->end();
  }
  std::string qbf_tag = "qbf:";
  std::string bfj_tag = "bfj:";
  std::string extra_tag = "extra:";
  std::string bfj = "brown fox jumped ";
  std::vector<cottontail::addr> p_quick;
  std::vector<cottontail::addr> q_jumped;
  std::shared_ptr<cottontail::Warren> warren;
  std::unique_ptr<cottontail::Hopper> hopper;
  {
    warren = cottontail::Warren::make(simple, burrow_name, &error);
    ASSERT_NE(warren, nullptr);
    warren->start();
    std::string quick = "quick";
    ASSERT_NE((hopper = warren->idx()->hopper(featurizer->featurize(quick))),
              nullptr);
    cottontail::addr p, q;
    ASSERT_TRUE(warren->annotator()->transaction());
    for (hopper->tau(cottontail::minfinity + 1, &p, &q);
         p < cottontail::maxfinity; hopper->tau(p + 1, &p, &q)) {
      ASSERT_TRUE(warren->annotator()->annotate(featurizer->featurize(qbf_tag),
                                                p, p + 2, 1.0 * p));
      p_quick.push_back(p);
    }
    cottontail::addr extra = warren->featurizer()->featurize(extra_tag);
    ASSERT_TRUE(
        warren->annotator()->annotate(extra, 10000000, 20000000, -11.0));
    std::string jumped = "jumped";
    ASSERT_NE((hopper = warren->idx()->hopper(featurizer->featurize(jumped))),
              nullptr);
    for (hopper->uat(cottontail::maxfinity - 1, &p, &q);
         q > cottontail::minfinity; hopper->uat(q - 1, &p, &q)) {
      ASSERT_TRUE(warren->annotator()->annotate(featurizer->featurize(bfj_tag),
                                                q - 2, q, -1.0 * q));
      q_jumped.push_back(q);
      ASSERT_EQ(warren->txt()->translate(q - 2, q), bfj);
    }
    ASSERT_EQ(q_jumped.size(), (size_t)NUMBER);
    ASSERT_TRUE(warren->annotator()->annotate(extra, 1, 1, 12.0));
    ASSERT_TRUE(warren->annotator()->annotate(extra, 9999, 99999, -12.0));
    ASSERT_TRUE(warren->annotator()->ready());
    warren->annotator()->commit();
    warren->idx()->reset();
    ASSERT_NE((hopper = warren->idx()->hopper(featurizer->featurize(qbf_tag))),
              nullptr);
    cottontail::fval v;
    for (auto k : p_quick) {
      hopper->tau(k, &p, &q, &v);
      EXPECT_EQ(p, k);
      EXPECT_EQ(q, k + 2);
      EXPECT_EQ(v, 1.0 * k);
    }
    std::string fox = "fox";
    ASSERT_NE((hopper = warren->idx()->hopper(featurizer->featurize(fox))),
              nullptr);
    cottontail::addr expected = 5;
    for (p = cottontail::minfinity; p < cottontail::maxfinity;) {
      hopper->tau(p + 1, &p, &q, &v);
      EXPECT_EQ(p, q);
      EXPECT_EQ(v, 0.0);
      if (p < cottontail::maxfinity) {
        EXPECT_EQ(p, expected);
      }
      expected += TOKENS;
    }
    EXPECT_EQ(expected, TOKENS * (NUMBER + 1) + 5);
    ASSERT_EQ(warren->idx()->count(featurizer->featurize(fox)), NUMBER);
    warren->end();
  }
  {
    warren = cottontail::Warren::make(simple, burrow_name, &error);
    warren->start();
    ASSERT_NE((hopper = warren->idx()->hopper(featurizer->featurize(qbf_tag))),
              nullptr);
    for (auto k : p_quick) {
      cottontail::addr p, q;
      cottontail::fval v;
      hopper->tau(k, &p, &q, &v);
      EXPECT_EQ(p, k);
      EXPECT_EQ(q, k + 2);
      EXPECT_EQ(v, 1.0 * k);
    }
    ASSERT_NE((hopper = warren->idx()->hopper(featurizer->featurize(bfj_tag))),
              nullptr);
    for (auto k : q_jumped) {
      cottontail::addr p, q;
      cottontail::fval v;
      hopper->rho(k, &p, &q, &v);
      EXPECT_EQ(p, k - 2);
      EXPECT_EQ(q, k);
      EXPECT_EQ(v, -1.0 * k);
    }
    cottontail::addr extra = warren->featurizer()->featurize(extra_tag);
    ASSERT_TRUE(warren->annotator()->transaction());
    ASSERT_TRUE(warren->annotator()->annotate(extra, 100, 200, -99.0));
    ASSERT_TRUE(warren->annotator()->ready());
    warren->annotator()->commit();
    warren->idx()->reset();
    {
      ASSERT_NE((hopper = warren->idx()->hopper(extra)), nullptr);
      cottontail::addr p, q;
      cottontail::fval v;
      hopper->tau(0, &p, &q, &v);
      EXPECT_EQ(p, 1);
      EXPECT_EQ(q, 1);
      EXPECT_EQ(v, 12.0);
      hopper->tau(99, &p, &q, &v);
      EXPECT_EQ(p, 100);
      EXPECT_EQ(q, 200);
      EXPECT_EQ(v, -99.0);
      hopper->tau(1000, &p, &q, &v);
      EXPECT_EQ(p, 9999);
      EXPECT_EQ(q, 99999);
      EXPECT_EQ(v, -12.0);
      hopper->tau(999999, &p, &q, &v);
      EXPECT_EQ(p, 10000000);
      EXPECT_EQ(q, 20000000);
      EXPECT_EQ(v, -11.0);
    }
    std::string fox = "fox";
    ASSERT_EQ(warren->idx()->count(featurizer->featurize(fox)), NUMBER);
    warren->end();
  }
}

// End-to-end testing simple index structures with concurrency

TEST(Simple, Concurrent) {
  std::string error;
  std::string burrow = cottontail::DEFAULT_BURROW;
  std::string options = "";
  std::shared_ptr<cottontail::Working> working =
      cottontail::Working::mkdir(burrow);
  ASSERT_NE(working, nullptr);
  std::shared_ptr<cottontail::Builder> builder =
      cottontail::SimpleBuilder::make(working, options, &error);
  ASSERT_NE(builder, nullptr);
  std::vector<std::string> text;
  text.push_back("test/ranking.txt");
  ASSERT_TRUE(cottontail::build_trec(text, builder, &error));
  std::string simple = "simple";
  std::shared_ptr<cottontail::Warren> warren;
  warren = cottontail::Warren::make(simple, burrow, &error);
  ASSERT_NE(warren, nullptr);
  auto solver = [&](int i) {
    std::shared_ptr<cottontail::Warren> larren = warren->clone();
    larren->start();
    sleep(i % 3);
    cottontail::addr p, q;
    if ((i / 3) % 2 == 0) {
      std::string query = "hello";
      std::unique_ptr<cottontail::Hopper> h =
          larren->hopper_from_gcl(query, &error);
      ASSERT_NE(h, nullptr);
      switch ((i / 6) % 5) {
      case 0:
        h->tau(19, &p, &q);
        ASSERT_EQ(p, 29);
        break;
      case 1:
        h->uat(40, &p, &q);
        ASSERT_EQ(q, 31);
        break;
      case 2:
        h->tau(6, &p, &q);
        ASSERT_EQ(p, 6);
        break;
      case 3:
        h->uat(16, &p, &q);
        ASSERT_EQ(q, 6);
        break;
      case 4:
        h->tau(0, &p, &q);
        ASSERT_EQ(p, 5);
        break;
      }
      std::string word = larren->txt()->translate(p, q);
      ASSERT_EQ(word.substr(0, 5), "hello");
    } else {
      std::string query = "world";
      std::unique_ptr<cottontail::Hopper> h =
          larren->hopper_from_gcl(query, &error);
      ASSERT_NE(h, nullptr);
      switch ((i / 6) % 5) {
      case 0:
        h->tau(31, &p, &q);
        ASSERT_EQ(p, 32);
        break;
      case 1:
        h->uat(61, &p, &q);
        ASSERT_EQ(q, 59);
        break;
      case 2:
        h->tau(34, &p, &q);
        ASSERT_EQ(p, 59);
        break;
      case 3:
        h->uat(58, &p, &q);
        ASSERT_EQ(q, 33);
        break;
      case 4:
        h->tau(0, &p, &q);
        ASSERT_EQ(p, 30);
        break;
      }
      std::string word = larren->txt()->translate(p, q);
      ASSERT_EQ(word.substr(0, 5), "world");
    }
    larren->end();
  };
  std::vector<std::thread> workers;
  for (int i = 0; i < 100; i++)
    workers.emplace_back(std::thread(solver, i));
  for (auto &worker : workers)
    worker.join();
}
