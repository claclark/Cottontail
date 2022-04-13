#include <fstream>
#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "src/compressor.h"
#include "src/simple_posting.h"

TEST(SimplePosting, Tokens) {
  std::string error;
  std::string empty_recipe = "";
  std::string test_filename = "test";
  std::string posting_compressor_name = "post";
  std::shared_ptr<cottontail::Compressor> posting_compressor =
      cottontail::Compressor::make(posting_compressor_name, empty_recipe,
                                   &error);
  ASSERT_NE(posting_compressor, nullptr) << error;
  std::string feature_compressor_name = "zlib";
  std::shared_ptr<cottontail::Compressor> feature_compressor =
      cottontail::Compressor::make(feature_compressor_name, empty_recipe,
                                   &error);
  ASSERT_NE(feature_compressor, nullptr) << error;
  std::shared_ptr<cottontail::SimplePostingFactory> factory =
      cottontail::SimplePostingFactory::make(posting_compressor,
                                             feature_compressor);
  ASSERT_NE(factory, nullptr);
  std::shared_ptr<cottontail::SimplePosting> posting1;
  std::shared_ptr<cottontail::SimplePosting> posting2;
  std::shared_ptr<cottontail::SimplePosting> posting3;
  {
    std::vector<cottontail::TokRecord> tv;
    tv.emplace_back(1, 1);
    tv.emplace_back(1, 14);
    tv.emplace_back(1, 20);
    tv.emplace_back(2, 2);
    tv.emplace_back(3, 4);
    tv.emplace_back(3, 5);
    tv.emplace_back(3, 6);
    std::vector<cottontail::TokRecord>::iterator it = tv.begin();
    posting1 = factory->posting_from_tokens(&it, tv.end());
    ASSERT_NE(posting1, nullptr);
    EXPECT_TRUE(posting1->invariants(&error)) << error;
    posting2 = factory->posting_from_tokens(&it, tv.end());
    ASSERT_NE(posting2, nullptr);
    EXPECT_TRUE(posting2->invariants(&error)) << error;
    posting3 = factory->posting_from_tokens(&it, tv.end());
    ASSERT_NE(posting3, nullptr);
    EXPECT_TRUE(posting3->invariants(&error)) << error;
  }
  {
    std::fstream out("temp", std::ios::binary | std::ios::out);
    posting1->write(&out);
    posting2->write(&out);
    posting3->write(&out);
  }
  std::shared_ptr<cottontail::SimplePosting> posting4;
  std::shared_ptr<cottontail::SimplePosting> posting5;
  std::shared_ptr<cottontail::SimplePosting> posting6;
  {
    std::fstream in("temp", std::ios::binary | std::ios::in);
    posting4 = factory->posting_from_file(&in);
    ASSERT_NE(posting4, nullptr);
    EXPECT_TRUE(posting4->invariants(&error)) << error;
    EXPECT_TRUE(*posting1 == *posting4);
    posting5 = factory->posting_from_file(&in);
    ASSERT_NE(posting5, nullptr);
    EXPECT_TRUE(posting5->invariants(&error)) << error;
    EXPECT_TRUE(*posting2 == *posting5);
    posting6 = factory->posting_from_file(&in);
    ASSERT_NE(posting6, nullptr);
    EXPECT_TRUE(posting6->invariants(&error)) << error;
    EXPECT_TRUE(*posting3 == *posting6);
  }
}

TEST(SimplePosting, Annotations) {
  std::string error;
  std::string empty_recipe = "";
  std::string test_filename = "test";
  std::string posting_compressor_name = "post";
  std::shared_ptr<cottontail::Compressor> posting_compressor =
      cottontail::Compressor::make(posting_compressor_name, empty_recipe,
                                   &error);
  ASSERT_NE(posting_compressor, nullptr) << error;
  std::string feature_compressor_name = "zlib";
  std::shared_ptr<cottontail::Compressor> feature_compressor =
      cottontail::Compressor::make(feature_compressor_name, empty_recipe,
                                   &error);
  ASSERT_NE(feature_compressor, nullptr) << error;
  std::shared_ptr<cottontail::SimplePostingFactory> factory =
      cottontail::SimplePostingFactory::make(posting_compressor,
                                             feature_compressor);
  ASSERT_NE(factory, nullptr);
  std::shared_ptr<cottontail::SimplePosting> posting1;
  std::shared_ptr<cottontail::SimplePosting> posting2;
  std::shared_ptr<cottontail::SimplePosting> posting3;
  {
    std::vector<cottontail::Annotation> ann;
    ann.emplace_back(1, 1, 4, 1.0);
    ann.emplace_back(1, 14, 22, 2.0);
    ann.emplace_back(1, 20, 24, 3.0);
    ann.emplace_back(2, 2, 2, 1.0);
    ann.emplace_back(3, 4, 10, 0.0);
    ann.emplace_back(3, 5, 11, 0.0);
    ann.emplace_back(3, 6, 12, 0.0);
    std::vector<cottontail::Annotation>::iterator it = ann.begin();
    posting1 = factory->posting_from_annotations(&it, ann.end());
    ASSERT_NE(posting1, nullptr);
    EXPECT_TRUE(posting1->invariants(&error)) << error;
    posting2 = factory->posting_from_annotations(&it, ann.end());
    ASSERT_NE(posting2, nullptr);
    EXPECT_TRUE(posting2->invariants(&error)) << error;
    posting3 = factory->posting_from_annotations(&it, ann.end());
    ASSERT_NE(posting3, nullptr);
    EXPECT_TRUE(posting3->invariants(&error)) << error;
  }
  {
    std::fstream out("temp", std::ios::binary | std::ios::out);
    posting1->write(&out);
    posting2->write(&out);
    posting3->write(&out);
  }
  std::shared_ptr<cottontail::SimplePosting> posting4;
  std::shared_ptr<cottontail::SimplePosting> posting5;
  std::shared_ptr<cottontail::SimplePosting> posting6;
  {
    std::fstream in("temp", std::ios::binary | std::ios::in);
    posting4 = factory->posting_from_file(&in);
    ASSERT_NE(posting4, nullptr);
    EXPECT_TRUE(posting4->invariants(&error)) << error;
    EXPECT_TRUE(*posting1 == *posting4);
    posting5 = factory->posting_from_file(&in);
    ASSERT_NE(posting5, nullptr);
    EXPECT_TRUE(posting5->invariants(&error)) << error;
    EXPECT_TRUE(*posting2 == *posting5);
    posting6 = factory->posting_from_file(&in);
    ASSERT_NE(posting6, nullptr);
    EXPECT_TRUE(posting6->invariants(&error)) << error;
    EXPECT_TRUE(*posting3 == *posting6);
  }
}

TEST(SimplePosting, Append) {
  std::string error;
  std::string empty_recipe = "";
  std::string test_filename = "test";
  std::string posting_compressor_name = "post";
  std::shared_ptr<cottontail::Compressor> posting_compressor =
      cottontail::Compressor::make(posting_compressor_name, empty_recipe,
                                   &error);
  ASSERT_NE(posting_compressor, nullptr) << error;
  std::string feature_compressor_name = "zlib";
  std::shared_ptr<cottontail::Compressor> feature_compressor =
      cottontail::Compressor::make(feature_compressor_name, empty_recipe,
                                   &error);
  ASSERT_NE(feature_compressor, nullptr) << error;
  std::shared_ptr<cottontail::SimplePostingFactory> factory =
      cottontail::SimplePostingFactory::make(posting_compressor,
                                             feature_compressor);
  ASSERT_NE(factory, nullptr);
  std::shared_ptr<cottontail::SimplePosting> posting1;
  std::shared_ptr<cottontail::SimplePosting> posting2;
  std::shared_ptr<cottontail::SimplePosting> posting3;
  std::vector<cottontail::Annotation> ann;
  ann.emplace_back(100, 1, 4, 1.0);
  ann.emplace_back(100, 14, 22, 2.0);
  ann.emplace_back(100, 20, 24, 3.0);
  std::vector<cottontail::Annotation>::iterator it = ann.begin();
  posting1 = factory->posting_from_annotations(&it, ann.end());
  ASSERT_NE(posting1, nullptr);
  EXPECT_TRUE(posting1->invariants(&error)) << error;
  posting2 = factory->posting_from_feature(100);
  ASSERT_NE(posting2, nullptr);
  EXPECT_TRUE(posting2->invariants(&error)) << error;
  posting2->append(posting1);
  EXPECT_TRUE(posting2->invariants(&error)) << error;
  EXPECT_TRUE(*posting1 == *posting2);
  ann.clear();
  ann.emplace_back(100, 100, 104, 1.0);
  ann.emplace_back(100, 114, 122, 2.0);
  ann.emplace_back(100, 120, 124, 3.0);
  it = ann.begin();
  posting3 = factory->posting_from_annotations(&it, ann.end());
  ASSERT_NE(posting3, nullptr);
  EXPECT_TRUE(posting3->invariants(&error)) << error;
  posting2->append(posting3);
  EXPECT_TRUE(posting2->invariants(&error)) << error;
}

TEST(SimplePosting, Mixed) {
  std::string error;
  std::string empty_recipe = "";
  std::string test_filename = "test";
  std::string posting_compressor_name = "post";
  std::shared_ptr<cottontail::Compressor> posting_compressor =
      cottontail::Compressor::make(posting_compressor_name, empty_recipe,
                                   &error);
  ASSERT_NE(posting_compressor, nullptr) << error;
  std::string feature_compressor_name = "zlib";
  std::shared_ptr<cottontail::Compressor> feature_compressor =
      cottontail::Compressor::make(feature_compressor_name, empty_recipe,
                                   &error);
  ASSERT_NE(feature_compressor, nullptr) << error;
  std::shared_ptr<cottontail::SimplePostingFactory> factory =
      cottontail::SimplePostingFactory::make(posting_compressor,
                                             feature_compressor);
  ASSERT_NE(factory, nullptr);
  std::shared_ptr<cottontail::SimplePosting> posting1;
  std::shared_ptr<cottontail::SimplePosting> posting2;
  std::shared_ptr<cottontail::SimplePosting> posting3;
  posting1 = factory->posting_from_feature(100);
  ASSERT_NE(posting1, nullptr);
  posting1->push(100, 100, 0.0);
  posting1->push(200, 300, 0.0);
  posting1->push(400, 500, 1.0);
  cottontail::addr p, q;
  cottontail::fval v;
  EXPECT_TRUE(posting1->get(0, &p, &q, &v));
  EXPECT_EQ(p, 100);
  EXPECT_EQ(q, 100);
  EXPECT_EQ(v, 0.0);
  EXPECT_TRUE(posting1->get(1, &p, &q, &v));
  EXPECT_EQ(p, 200);
  EXPECT_EQ(q, 300);
  EXPECT_EQ(v, 0.0);
  EXPECT_TRUE(posting1->get(2, &p, &q, &v));
  EXPECT_EQ(p, 400);
  EXPECT_EQ(q, 500);
  EXPECT_EQ(v, 1.0);
  EXPECT_TRUE(posting1->invariants(&error)) << error;
  posting2 = factory->posting_from_feature(100);
  ASSERT_NE(posting2, nullptr);
  posting2->push(10, 10, 0.0);
  posting2->push(11, 11, 0.0);
  EXPECT_TRUE(posting2->get(0, &p, &q, &v));
  EXPECT_EQ(p, 10);
  EXPECT_EQ(q, 10);
  EXPECT_EQ(v, 0.0);
  EXPECT_TRUE(posting2->get(1, &p, &q, &v));
  EXPECT_EQ(p, 11);
  EXPECT_EQ(q, 11);
  EXPECT_EQ(v, 0.0);
  EXPECT_TRUE(posting2->invariants(&error)) << error;
  posting2->append(posting1);
  EXPECT_TRUE(posting2->get(0, &p, &q, &v));
  EXPECT_EQ(p, 10);
  EXPECT_EQ(q, 10);
  EXPECT_EQ(v, 0.0);
  EXPECT_TRUE(posting2->get(1, &p, &q, &v));
  EXPECT_EQ(p, 11);
  EXPECT_EQ(q, 11);
  EXPECT_EQ(v, 0.0);
  EXPECT_TRUE(posting2->get(2, &p, &q, &v));
  EXPECT_EQ(p, 100);
  EXPECT_EQ(q, 100);
  EXPECT_EQ(v, 0.0);
  EXPECT_TRUE(posting2->get(3, &p, &q, &v));
  EXPECT_EQ(p, 200);
  EXPECT_EQ(q, 300);
  EXPECT_EQ(v, 0.0);
  EXPECT_TRUE(posting2->get(4, &p, &q, &v));
  EXPECT_EQ(p, 400);
  EXPECT_EQ(q, 500);
  EXPECT_EQ(v, 1.0);
  EXPECT_TRUE(posting2->invariants(&error)) << error;
  EXPECT_FALSE(posting2->get(5, &p, &q, &v));
  posting3 = factory->posting_from_feature(100);
  ASSERT_NE(posting3, nullptr);
  posting3->push(1000, 1000, 0.0);
  posting3->push(1100, 1100, 0.0);
  posting1->append(posting3);
  EXPECT_TRUE(posting1->get(0, &p, &q, &v));
  EXPECT_EQ(p, 100);
  EXPECT_EQ(q, 100);
  EXPECT_EQ(v, 0.0);
  EXPECT_TRUE(posting1->get(1, &p, &q, &v));
  EXPECT_EQ(p, 200);
  EXPECT_EQ(q, 300);
  EXPECT_EQ(v, 0.0);
  EXPECT_TRUE(posting1->get(2, &p, &q, &v));
  EXPECT_EQ(p, 400);
  EXPECT_EQ(q, 500);
  EXPECT_EQ(v, 1.0);
  EXPECT_TRUE(posting1->get(3, &p, &q, &v));
  EXPECT_EQ(p, 1000);
  EXPECT_EQ(q, 1000);
  EXPECT_EQ(v, 0.0);
  EXPECT_TRUE(posting1->get(4, &p, &q, &v));
  EXPECT_EQ(p, 1100);
  EXPECT_EQ(q, 1100);
  EXPECT_EQ(v, 0.0);
  EXPECT_TRUE(posting1->invariants(&error)) << error;
  EXPECT_FALSE(posting2->get(5, &p, &q, &v));
  posting1->push(2000, 2100, 1.0);
  EXPECT_TRUE(posting1->get(0, &p, &q, &v));
  EXPECT_EQ(p, 100);
  EXPECT_EQ(q, 100);
  EXPECT_EQ(v, 0.0);
  EXPECT_TRUE(posting1->get(1, &p, &q, &v));
  EXPECT_EQ(p, 200);
  EXPECT_EQ(q, 300);
  EXPECT_EQ(v, 0.0);
  EXPECT_TRUE(posting1->get(2, &p, &q, &v));
  EXPECT_EQ(p, 400);
  EXPECT_EQ(q, 500);
  EXPECT_EQ(v, 1.0);
  EXPECT_TRUE(posting1->get(3, &p, &q, &v));
  EXPECT_EQ(p, 1000);
  EXPECT_EQ(q, 1000);
  EXPECT_EQ(v, 0.0);
  EXPECT_TRUE(posting1->get(4, &p, &q, &v));
  EXPECT_EQ(p, 1100);
  EXPECT_EQ(q, 1100);
  EXPECT_EQ(v, 0.0);
  EXPECT_TRUE(posting1->get(5, &p, &q, &v));
  EXPECT_EQ(p, 2000);
  EXPECT_EQ(q, 2100);
  EXPECT_EQ(v, 1.0);
  EXPECT_TRUE(posting1->invariants(&error)) << error;
  EXPECT_FALSE(posting2->get(6, &p, &q, &v));
  EXPECT_FALSE(posting2->get(100000, &p, &q, &v));
}

TEST(SimplePosting, Merge) {
  std::string empty_recipe = "";
  std::string test_filename = "test";
  std::string posting_compressor_name = "post";
  std::shared_ptr<cottontail::Compressor> posting_compressor =
      cottontail::Compressor::make(posting_compressor_name, empty_recipe);
  ASSERT_NE(posting_compressor, nullptr);
  std::string feature_compressor_name = "zlib";
  std::shared_ptr<cottontail::Compressor> feature_compressor =
      cottontail::Compressor::make(feature_compressor_name, empty_recipe);
  ASSERT_NE(feature_compressor, nullptr);
  std::shared_ptr<cottontail::SimplePostingFactory> factory =
      cottontail::SimplePostingFactory::make(posting_compressor,
                                             feature_compressor);
  std::vector<std::shared_ptr<cottontail::SimplePosting>> postings;
  std::shared_ptr<cottontail::SimplePosting> posting1 =
      factory->posting_from_feature(100);
  ASSERT_NE(posting1, nullptr);
  postings.push_back(posting1);
  posting1->push(100, 300, 10.0);
  posting1->push(400, 600, 100000.0);
  posting1->push(700, 900, 777.0);
  std::shared_ptr<cottontail::SimplePosting> posting2 =
      factory->posting_from_feature(100);
  ASSERT_NE(posting2, nullptr);
  postings.push_back(posting2);
  posting2->push(200, 400, 0.0);
  posting2->push(400, 600, 1000.0);
  posting2->push(700, 900, 111.0);
  posting2->push(800, 1000, -1.5);
  std::shared_ptr<cottontail::SimplePosting> posting3 =
      factory->posting_from_feature(100);
  postings.push_back(posting3);
  postings.push_back(posting2);
  ASSERT_NE(posting3, nullptr);
  posting3->push(1, 9, 0.0);
  posting3->push(400, 600, 1000.0);
  posting3->push(2000, 3000, -99.0);
  std::shared_ptr<cottontail::SimplePosting> posting4 =
      factory->posting_from_merge(postings);
  ASSERT_TRUE(posting4->invariants());
  cottontail::addr p, q;
  cottontail::fval v;
  EXPECT_TRUE(posting4->get(0, &p, &q, &v));
  EXPECT_EQ(p, 1);
  EXPECT_EQ(q, 9);
  EXPECT_EQ(v, 0.0);
  EXPECT_TRUE(posting4->get(1, &p, &q, &v));
  EXPECT_EQ(p, 100);
  EXPECT_EQ(q, 300);
  EXPECT_EQ(v, 10.0);
  EXPECT_TRUE(posting4->get(2, &p, &q, &v));
  EXPECT_EQ(p, 200);
  EXPECT_EQ(q, 400);
  EXPECT_EQ(v, 0.0);
  EXPECT_TRUE(posting4->get(3, &p, &q, &v));
  EXPECT_EQ(p, 400);
  EXPECT_EQ(q, 600);
  EXPECT_EQ(v, 103000.0);
  EXPECT_TRUE(posting4->get(4, &p, &q, &v));
  EXPECT_EQ(p, 700);
  EXPECT_EQ(q, 900);
  EXPECT_EQ(v, 999.0);
  EXPECT_TRUE(posting4->get(5, &p, &q, &v));
  EXPECT_EQ(p, 800);
  EXPECT_EQ(q, 1000);
  EXPECT_EQ(v, -3.0);
  EXPECT_TRUE(posting4->get(6, &p, &q, &v));
  EXPECT_EQ(p, 2000);
  EXPECT_EQ(q, 3000);
  EXPECT_EQ(v, -99.0);
  EXPECT_FALSE(posting4->get(7, &p, &q, &v));
  std::shared_ptr<cottontail::SimplePosting> posting5 =
      factory->posting_from_feature(100);
  posting5->push(1000000, 1000000, 1000000.0);
  posting5->push(2000000, 3000000, -1000000.0);
  postings.clear();
  postings.push_back(posting4);
  postings.push_back(posting5);
  std::shared_ptr<cottontail::SimplePosting> posting6 =
      factory->posting_from_merge(postings);
  EXPECT_TRUE(posting6->get(0, &p, &q, &v));
  EXPECT_EQ(p, 1);
  EXPECT_EQ(q, 9);
  EXPECT_EQ(v, 0.0);
  EXPECT_TRUE(posting6->get(1, &p, &q, &v));
  EXPECT_EQ(p, 100);
  EXPECT_EQ(q, 300);
  EXPECT_EQ(v, 10.0);
  EXPECT_TRUE(posting6->get(2, &p, &q, &v));
  EXPECT_EQ(p, 200);
  EXPECT_EQ(q, 400);
  EXPECT_EQ(v, 0.0);
  EXPECT_TRUE(posting6->get(3, &p, &q, &v));
  EXPECT_EQ(p, 400);
  EXPECT_EQ(q, 600);
  EXPECT_EQ(v, 103000.0);
  EXPECT_TRUE(posting6->get(4, &p, &q, &v));
  EXPECT_EQ(p, 700);
  EXPECT_EQ(q, 900);
  EXPECT_EQ(v, 999.0);
  EXPECT_TRUE(posting6->get(5, &p, &q, &v));
  EXPECT_EQ(p, 800);
  EXPECT_EQ(q, 1000);
  EXPECT_EQ(v, -3.0);
  EXPECT_TRUE(posting6->get(6, &p, &q, &v));
  EXPECT_EQ(p, 2000);
  EXPECT_EQ(q, 3000);
  EXPECT_EQ(v, -99.0);
  EXPECT_TRUE(posting6->get(7, &p, &q, &v));
  EXPECT_EQ(p, 1000000);
  EXPECT_EQ(q, 1000000);
  EXPECT_EQ(v, 1000000.0);
  EXPECT_TRUE(posting6->get(8, &p, &q, &v));
  EXPECT_EQ(p, 2000000);
  EXPECT_EQ(q, 3000000);
  EXPECT_EQ(v, -1000000.0);
  EXPECT_FALSE(posting6->get(9, &p, &q, &v));
  EXPECT_FALSE(posting6->get(1000000, &p, &q, &v));
}
