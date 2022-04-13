#include <memory>
#include <vector>

#include "gtest/gtest.h"

#include "src/cottontail.h"

TEST(Eval, Trim) {
  ASSERT_EQ(cottontail::trec_docno(" <DOCNO> hello </DOCNO> "), "hello");
  ASSERT_EQ(cottontail::trec_docno("MARCO_100  ))) "), "MARCO_100");
}

TEST(Eval, Metrics) {
  std::string error;
  std::string burrow = cottontail::DEFAULT_BURROW;
  std::string options = "";
  std::shared_ptr<cottontail::Working> working =
      cottontail::Working::mkdir(burrow);
  ASSERT_NE(working, nullptr);
  std::shared_ptr<cottontail::Builder> builder =
      cottontail::SimpleBuilder::make(working, options, &error);
  ASSERT_NE(builder, nullptr) << error;
  std::vector<std::string> text;
  text.push_back("test/ranking.txt");
  ASSERT_TRUE(cottontail::build_trec(text, builder, &error)) << error;
  std::string simple = "simple";
  std::shared_ptr<cottontail::Warren> warren;
  {
    warren = cottontail::Warren::make(simple, burrow, &error);
    ASSERT_NE(warren, nullptr) << error;
    warren->start();
    std::string container = "(... <DOC> </DOC>)";
    ASSERT_TRUE(warren->set_default_container(container, &error)) << error;
    std::string id_key = "id";
    std::string id_value = "(... <DOCNO> </DOCNO>)";
    ASSERT_TRUE(warren->set_parameter(id_key, id_value, &error)) << error;
    ASSERT_TRUE(tf_idf_annotations(warren, &error)) << error;
    warren->end();
  }
  warren = cottontail::Warren::make(simple, burrow, &error);
  ASSERT_NE(warren, nullptr) << error;
  warren->start();
  std::map<std::string, std::map<cottontail::addr, cottontail::fval>> qrels;
  std::map<std::string, std::map<cottontail::addr, cottontail::addr>> lengths;
  std::map<std::string, std::map<cottontail::addr, std::string>> docnos;
  std::string qrels_filename = "test/test.qrels";
  ASSERT_TRUE(load_trec_qrels(warren, qrels_filename, &qrels, &lengths, &docnos,
                              &error)) << error;
  EXPECT_EQ(qrels["one"][0], 0.5);
  EXPECT_EQ(qrels["one"][35], 0.5);
  EXPECT_EQ(qrels["one"][54], 0.0);
  EXPECT_EQ(qrels["two"][0], 0.0);
  EXPECT_EQ(qrels["two"][24], 0.5);
  std::string query = "cat in the hat";
  std::vector<cottontail::RankingResult> results =
      cottontail::bm25_ranking(warren, query);
  std::map<std::string, cottontail::fval> metrics;
  cottontail::eval(qrels["one"], results, &metrics);
  EXPECT_FLOAT_EQ(metrics["ap"], 0.5);
  EXPECT_FLOAT_EQ(metrics["dcg"], 0.5);
  EXPECT_FLOAT_EQ(metrics["ndcg"], 0.61314719276545837);
  EXPECT_FLOAT_EQ(metrics["p05"], 0.2);
  EXPECT_FLOAT_EQ(metrics["p10"], 0.1);
  EXPECT_FLOAT_EQ(metrics["p20"], 0.05);
  EXPECT_FLOAT_EQ(metrics["precision"], 1);
  EXPECT_FLOAT_EQ(metrics["rbp50"], 0.25);
  EXPECT_FLOAT_EQ(metrics["rbp95"], 0.025);
  EXPECT_FLOAT_EQ(metrics["recall"], 0.5);
  EXPECT_FLOAT_EQ(metrics["rel"], 2);
  EXPECT_FLOAT_EQ(metrics["rel-ret"], 1);
  EXPECT_FLOAT_EQ(metrics["ret"], 1);
  EXPECT_FLOAT_EQ(metrics["tbg"], 0.4928);
  warren->end();
}
