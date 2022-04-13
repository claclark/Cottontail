#include "gtest/gtest.h"

#include "src/cottontail.h"

TEST(StemmerTest, Null) {
  cottontail::NullStemmer ns;
  EXPECT_EQ(ns.stem(""), "");
  EXPECT_EQ(ns.stem("a"), "a");
  EXPECT_EQ(ns.stem("at"), "at");
  EXPECT_EQ(ns.stem("the"), "the");
  EXPECT_EQ(ns.stem("testing"), "testing");

  bool stemmed;
  EXPECT_EQ(ns.stem("testing", &stemmed), "testing");
  EXPECT_FALSE(stemmed);
}

TEST(StemmerTest, Porter) {
  cottontail::Porter p;
  EXPECT_EQ(p.stem("tests"), "porter:test");
  EXPECT_EQ(p.stem("testing"), "porter:test");
  EXPECT_EQ(p.stem("ponies"), "porter:poni");
  EXPECT_EQ(p.stem("PONIES"), "porter:poni");
  EXPECT_EQ(p.stem("orienteering"), "porter:orient");
  EXPECT_EQ(p.stem("oriental"), "porter:orient");

  // Under Porter marine vegetation and marinated vegetable are the same thing
  EXPECT_EQ(p.stem("marine"), "porter:marin");
  EXPECT_EQ(p.stem("vegetation"), "porter:veget");

  EXPECT_EQ(p.stem("marinated"), "porter:marin");
  EXPECT_EQ(p.stem("vegetables"), "porter:veget");

  EXPECT_EQ(p.stem(""), "");
  EXPECT_EQ(p.stem("a"), "a");
  EXPECT_EQ(p.stem("at"), "at");
  EXPECT_EQ(p.stem("the"), "porter:the");
  EXPECT_EQ(p.stem("888"), "888");
  EXPECT_EQ(p.stem("abc123"), "abc123");
  EXPECT_EQ(p.stem("porter:marin"), "porter:marin"); // idempotent

  bool stemmed;
  EXPECT_EQ(p.stem("at", &stemmed), "at");
  EXPECT_FALSE(stemmed);
  EXPECT_EQ(p.stem("testing", &stemmed), "porter:test");
  EXPECT_TRUE(stemmed);
}
