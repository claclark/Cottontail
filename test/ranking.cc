#include <memory>
#include <vector>

#include "gtest/gtest.h"

#include "src/cottontail.h"

TEST(Ranking, Algorithms) {
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
  std::string container = "(... <DOC> </DOC>)";
  {
    warren = cottontail::Warren::make(simple, burrow, &error);
    ASSERT_NE(warren, nullptr);
    warren->start();
    warren->set_default_container(container);
    ASSERT_TRUE(tf_idf_annotations(warren, &error));
    warren->end();
  }
  warren = cottontail::Warren::make(simple, burrow, &error);
  warren->start();
  std::string gcl = "tf:hello";
  std::unique_ptr<cottontail::Hopper> hello =
      warren->hopper_from_gcl(gcl, &error);
  ASSERT_NE(hello, nullptr);
  cottontail::addr k = cottontail::minfinity + 1, p, q;
  cottontail::fval v;
  hello->tau(k, &p, &q, &v);
  EXPECT_EQ(p, 0);
  EXPECT_EQ(q, 19);
  EXPECT_EQ(v, 3.0);
  hello->tau(p + 1, &p, &q, &v);
  EXPECT_EQ(p, 24);
  EXPECT_EQ(q, 34);
  EXPECT_EQ(v, 2.0);
  hello->tau(p + 1, &p, &q, &v);
  EXPECT_EQ(p, 35);
  EXPECT_EQ(q, 53);
  EXPECT_EQ(v, 1.0);
  hello->tau(p + 1, &p, &q, &v);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  EXPECT_EQ(v, 0.0);
  gcl = "idf:hello";
  hello = warren->hopper_from_gcl(gcl, &error);
  hello->tau(0, &p, &q, &v);
  EXPECT_EQ(p, 0);
  EXPECT_EQ(q, 0);
  EXPECT_FLOAT_EQ(v, 0.287682);
  gcl = "idf:world";
  hello = warren->hopper_from_gcl(gcl, &error);
  hello->tau(0, &p, &q, &v);
  EXPECT_EQ(p, 0);
  EXPECT_EQ(q, 0);
  EXPECT_FLOAT_EQ(v, 0.693147);
  gcl = "rsj:quick";
  hello = warren->hopper_from_gcl(gcl, &error);
  hello->tau(0, &p, &q, &v);
  EXPECT_EQ(p, 0);
  EXPECT_EQ(q, 0);
  EXPECT_FLOAT_EQ(v, 0.847298);
  gcl = "avgl:avgl";
  hello = warren->hopper_from_gcl(gcl, &error);
  hello->tau(0, &p, &q, &v);
  EXPECT_EQ(p, 0);
  EXPECT_EQ(q, 0);
  EXPECT_EQ(v, 16.0);
  std::string query = "hello world";
  std::map<std::string, cottontail::fval> parameters;
  parameters["mu"] = 16.0;
  std::vector<cottontail::RankingResult> results =
      cottontail::lmd_ranking(warren, query, parameters);
  EXPECT_EQ(results.size(), (size_t) 3);
  gcl = "(... <DOCNO> </DOCNO>)";
  std::unique_ptr<cottontail::Hopper> docno =
      warren->hopper_from_gcl(gcl, &error);
  ASSERT_NE(docno, nullptr);
  docno->tau(results[0].p(), &p, &q);
  std::string topdoc = warren->txt()->translate(p, q);
  EXPECT_EQ(topdoc, "<DOCNO> doc-001 </DOCNO>\n");
  std::string query2 = "cat in the hat";
  std::vector<cottontail::RankingResult> results2 =
      cottontail::bm25_ranking(warren, query2);
  EXPECT_EQ(results2.size(), (size_t) 1);
  docno->tau(results2[0].p(), &p, &q);
  topdoc = warren->txt()->translate(p, q);
  EXPECT_EQ(topdoc, "<DOCNO> doc-002 </DOCNO>\n");
  std::vector<cottontail::RankingResult> results3 =
      cottontail::icover_ranking(warren, query, container);
  EXPECT_EQ(results3.size(), (size_t) 4);
  docno->tau(results3[0].container_p(), &p, &q);
  topdoc = warren->txt()->translate(p, q);
  EXPECT_EQ(topdoc, "<DOCNO> doc-001 </DOCNO>\n");
  std::vector<std::string> terms = rsj_prf(warren, results3);
  EXPECT_NE(terms.size(), (size_t) 0);
  terms = qa_prf(warren, results3);
  EXPECT_NE(terms.size(), (size_t) 0);
  terms = kld_prf(warren, results3);
  EXPECT_NE(terms.size(), (size_t) 0);
  std::vector<cottontail::RankingResult> results4 =
      cottontail::tiered_ranking(warren, query, container);
  EXPECT_EQ(results4.size(), (size_t) 3);
  docno->tau(results4[0].p(), &p, &q);
  topdoc = warren->txt()->translate(p, q);
  EXPECT_EQ(topdoc, "<DOCNO> doc-001 </DOCNO>\n");
  std::string gcl2 = "(^ hello world)";
  std::vector<cottontail::RankingResult> results5 =
      cottontail::ssr_ranking(warren, gcl2, container);
  EXPECT_EQ(results5.size(), (size_t) 1);
  docno->tau(results5[0].p(), &p, &q);
  topdoc = warren->txt()->translate(p, q);
  EXPECT_EQ(topdoc, "<DOCNO> doc-001 </DOCNO>\n");
  warren->end();
}

TEST(Ranking, DF) {
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
  std::string container = "(... <DOC> </DOC>)";
  {
    warren = cottontail::Warren::make(simple, burrow, &error);
    ASSERT_NE(warren, nullptr);
    warren->start();
    warren->set_default_container(container);
    ASSERT_TRUE(tf_df_annotations(warren, &error));
    warren->end();
  }
  warren = cottontail::Warren::make(simple, burrow, &error);
  ASSERT_NE(warren, nullptr);
  warren->start();
  std::string query = "cat in the hat";
  std::vector<cottontail::RankingResult> results =
      cottontail::bm25_ranking(warren, query);
  EXPECT_EQ(results.size(), (size_t) 1);
  std::string gcl = "(... <DOCNO> </DOCNO>)";
  std::unique_ptr<cottontail::Hopper> docno =
      warren->hopper_from_gcl(gcl, &error);
  ASSERT_NE(docno, nullptr);
  cottontail::addr p, q;
  docno->tau(results[0].p(), &p, &q);
  std::string topdoc = warren->txt()->translate(p, q);
  EXPECT_EQ(topdoc, "<DOCNO> doc-002 </DOCNO>\n");
  warren->end();
}
