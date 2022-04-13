#include "src/parameters.h"

#include "gtest/gtest.h"

#include "src/cottontail.h"
#include "src/ranking.h"

TEST(ParameterTest, RandomParameter) {
  cottontail::UniformRandomParameter urp;
  for (unsigned i = 0; i < 100; i++) {
    cottontail::fval v = urp();
    EXPECT_GE(v, 0.0);
    EXPECT_LT(v, 1.0);
  }
  for (unsigned i = 0; i < 100; i++) {
    cottontail::UniformRandomParameter urp(i, i + 1.0);
    cottontail::fval v = urp();
    EXPECT_GE(v, i);
    EXPECT_LT(v, i + 1.0);
  }
  for (unsigned i = 0; i < 100; i++) {
    cottontail::UniformRandomParameter urp(i, i + 10.0);
    cottontail::fval v = urp();
    EXPECT_GE(v, i);
    EXPECT_LT(v, i + 10.0);
  }
  cottontail::UniformRandomParameter lurp(0.01, 1000.0);
  for (unsigned i = 0; i < 100; i++) {
    cottontail::fval v = lurp();
    EXPECT_GE(v, 0.01);
    EXPECT_LT(v, 1000.0);
  }
}

TEST(ParameterTest, Tiered) {
  std::map<std::string, cottontail::fval> tiered;
  tiered = cottontail::Parameters::from_ranker_name("tiered")->random();
  EXPECT_GE(tiered["tiered:delta"], 0.0);
  tiered = cottontail::Parameters::from_ranker_name("tiered")->defaults();
  EXPECT_GE(tiered["tiered:delta"], 0.0);
}

TEST(ParameterTest, SSR) {
  std::map<std::string, cottontail::fval> ssr;
  ssr = cottontail::Parameters::from_ranker_name("ssr")->random();
  EXPECT_GE(ssr["ssr:K"], 0.0);
  ssr = cottontail::Parameters::from_ranker_name("ssr")->defaults();
  EXPECT_GE(ssr["ssr:K"], 0.0);
}

TEST(ParameterTest, BM25) {
  std::map<std::string, cottontail::fval> bm25;
  bm25 = cottontail::Parameters::from_ranker_name("bm25")->random();
  EXPECT_GE(bm25["bm25:b"], 0.0);
  EXPECT_LT(bm25["bm25:b"], 1.0);
  EXPECT_GT(bm25["bm25:k1"], 0.0);
  bm25 = cottontail::Parameters::from_ranker_name("bm25")->defaults();
  EXPECT_GE(bm25["bm25:b"], 0.0);
  EXPECT_LT(bm25["bm25:b"], 1.0);
  EXPECT_GT(bm25["bm25:k1"], 0.0);
}

TEST(ParameterTest, LMD) {
  std::map<std::string, cottontail::fval> lmd;
  lmd = cottontail::Parameters::from_ranker_name("lmd")->random();
  EXPECT_GE(lmd["lmd:mu"], 0.0);
  lmd = cottontail::Parameters::from_ranker_name("lmd")->defaults();
  EXPECT_GE(lmd["lmd:mu"], 0.0);
}

