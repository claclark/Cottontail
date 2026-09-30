#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "src/cottontail.h"
#include "src/nlohmann.h"

namespace {

using Queries = std::map<std::string, std::map<std::string, cottontail::fval>>;
using Results = std::map<std::string, std::vector<std::string>>;

class Splade : public ::testing::Test {
protected:
  void SetUp() override {
    auto featurizer = cottontail::Featurizer::make("hashing", "");
    auto tokenizer = cottontail::Tokenizer::make("utf8", "");
    ASSERT_NE(featurizer, nullptr);
    ASSERT_NE(tokenizer, nullptr);
    warren = cottontail::Bigwig::make(nullptr, featurizer, tokenizer,
                                    cottontail::Fluffle::make(), &error);
    ASSERT_NE(warren, nullptr) << error;
    warren->merge(false);
  }

  bool append(const std::string &record, const std::string &feature = ":") {
    cottontail::addr p, q;
    if (!warren->transaction(&error) ||
        !cottontail::json_append(record, warren, &p, &q, feature, &error) ||
        !warren->ready(&error))
      return false;
    warren->commit();
    return true;
  }

  void populate() {
    ASSERT_TRUE(append(R"({"docid":"A","raw_text":"irrelevant",
                           "splade_vector":{"x":2,"##n":1}})")) << error;
    ASSERT_TRUE(append(R"({"docid":"B",
                           "splade_vector":{"x":1,"##n":4}})")) << error;
    ASSERT_TRUE(append(R"({"docid":"C","splade_vector":{"x":3}})")) << error;
    ASSERT_TRUE(append(R"({"docid":"D","splade_vector":{}})")) << error;
  }

  Results identifiers(Results results) {
    for (auto &topic : results)
      for (auto &id : topic.second)
        id = cottontail::trec_docno(id);
    return results;
  }

  std::shared_ptr<cottontail::Bigwig> warren;
  std::string error;
};

TEST_F(Splade, DefaultsAndThreads) {
  populate();
  Queries queries = {{"both", {{"x", 2}, {"##n", 1}}},
                     {"fractional", {{"x", 0.5}, {"##n", 0.25}}},
                     {"x", {{"x", 1}}},
                     {"subword", {{"##n", 1}}},
                     {"empty", {}},
                     {"missing", {{"no such feature", 1}}},
                     {"zero", {{"x", 0}}}};
  Results expected = {{"both", {"B", "C", "A"}},
                      {"fractional", {"B", "C", "A"}},
                      {"x", {"C", "A", "B"}},
                      {"subword", {"B", "A"}},
                      {"empty", {}}, {"missing", {}}, {"zero", {}}};
  Results results;
  cottontail::addr elapsed = -1;
  ASSERT_TRUE(cottontail::splade(warren, queries, &results, &error, 1, &elapsed))
      << error;
  EXPECT_EQ(identifiers(results), expected);
  EXPECT_GE(elapsed, 0);
  EXPECT_FALSE(warren->started());
  Results parallel;
  ASSERT_TRUE(cottontail::splade(warren, queries, &parallel, nullptr, 4));
  EXPECT_EQ(parallel, results);
  ASSERT_TRUE(cottontail::splade(warren, queries, &parallel));
  EXPECT_EQ(parallel, results);
}

TEST_F(Splade, DepthAndEmptyInputs) {
  populate();
  Queries queries = {{"q", {{"x", 2}, {"##n", 1}}}};
  Results results;
  ASSERT_TRUE(cottontail::splade(warren, queries, {{"depth", "1"}}, &results));
  EXPECT_EQ(identifiers(results)["q"], std::vector<std::string>({"B"}));
  ASSERT_TRUE(cottontail::splade(warren, queries, {{"depth", "0"}}, &results));
  EXPECT_TRUE(results["q"].empty());
  cottontail::addr elapsed = -1;
  ASSERT_TRUE(cottontail::splade(warren, {}, &results, nullptr, 0, &elapsed));
  EXPECT_TRUE(results.empty());
  EXPECT_EQ(elapsed, 0);
  EXPECT_FALSE(warren->started());
}

TEST_F(Splade, ParametersAndContainerScoping) {
  ASSERT_TRUE(append(R"({"key":"before","v":{"##n":-1,"x":1000}})", "@"));
  ASSERT_TRUE(append(R"({"key":"A","v":{"##n":1,"x":0.5}})", "selected"));
  ASSERT_TRUE(append(R"({"key":"between","v":{"x":-1,"##n":1000}})", "@"));
  ASSERT_TRUE(append(R"({"key":"B","v":{"##n":0.5,"x":1}})", "selected"));
  // The same features outside the selected containers must not score.
  ASSERT_TRUE(append(R"({"key":"outside","v":{"##n":1000}})", "@"));
  Queries queries = {{"q", {{"##n", 2}, {"x", 1}}}};
  std::map<std::string, std::string> parameters = {
      {"container", "selected"}, {"prefix", ":v:"}, {"id", ":key:"}};
  Results results;
  ASSERT_TRUE(cottontail::splade(warren, queries, parameters, &results, &error))
      << error;
  EXPECT_EQ(identifiers(results)["q"], std::vector<std::string>({"A", "B"}));
  parameters["container"] = "(<< selected selected)";
  ASSERT_TRUE(cottontail::splade(warren, queries, parameters, &results, &error))
      << error;
  EXPECT_EQ(identifiers(results)["q"], std::vector<std::string>({"A", "B"}));
  parameters["id"] = "missing";
  ASSERT_TRUE(cottontail::splade(warren, queries, parameters, &results));
  EXPECT_TRUE(results["q"].empty());
}

TEST_F(Splade, ReordersAndExhaustsHoppers) {
  // Physical field order differs from query order and changes between records.
  ASSERT_TRUE(append(R"({"docid":"A","splade_vector":{"z":1,"a":1,"m":1}})"));
  ASSERT_TRUE(append(R"({"docid":"empty","splade_vector":{}})"));
  ASSERT_TRUE(append(R"({"docid":"B","splade_vector":{"m":5}})"));
  ASSERT_TRUE(append(R"({"docid":"C","splade_vector":{"m":2,"z":3}})"));
  ASSERT_TRUE(append(R"({"docid":"D","splade_vector":{"a":6}})"));
  ASSERT_TRUE(append(R"({"docid":"zero","splade_vector":{"a":0}})"));
  Queries queries = {{"all", {{"a", 1}, {"m", 1}, {"z", 1}, {"missing", 1}}},
                     {"single", {{"a", 1}}}};
  Results results;
  ASSERT_TRUE(cottontail::splade(warren, queries, &results, &error))
      << error;
  EXPECT_EQ(identifiers(results)["all"],
            std::vector<std::string>({"D", "B", "C", "A"}));
  EXPECT_EQ(identifiers(results)["single"], std::vector<std::string>({"D", "A"}));
  ASSERT_TRUE(cottontail::splade(warren, queries, {{"depth", "2"}},
                                &results, &error)) << error;
  EXPECT_EQ(identifiers(results)["all"], std::vector<std::string>({"D", "B"}));
}

TEST_F(Splade, PreservesReadEpoch) {
  populate();
  warren->start();
  ASSERT_TRUE(append(R"({"docid":"new","splade_vector":{"x":100}})"));
  Results results;
  Queries queries = {{"q", {{"x", 1}}}};
  ASSERT_TRUE(cottontail::splade(warren, queries, &results, &error)) << error;
  EXPECT_TRUE(warren->started());
  EXPECT_EQ(identifiers(results)["q"], std::vector<std::string>({"C", "A", "B"}));
  warren->end();
  ASSERT_TRUE(cottontail::splade(warren, queries, &results, &error)) << error;
  EXPECT_FALSE(warren->started());
  EXPECT_EQ(identifiers(results)["q"],
            std::vector<std::string>({"new", "C", "A", "B"}));
}

TEST_F(Splade, ValidationAndNullError) {
  populate();
  Queries queries = {{"q", {{"x", 1}}}};
  Results results;
  for (const std::string depth : {"", "-1", "1.5", "2x", " 2",
                                   "184467440737095516160"})
    EXPECT_FALSE(cottontail::splade(warren, queries, {{"depth", depth}}, &results));
  EXPECT_FALSE(cottontail::splade(warren, queries, {{"oops", "1"}}, &results));
  EXPECT_FALSE(cottontail::splade(warren, queries, {{"container", ""}}, &results));
  EXPECT_FALSE(cottontail::splade(warren, queries, {{"container", "("}}, &results));
  EXPECT_FALSE(cottontail::splade(warren, queries, {{"id", "("}}, &results));
  EXPECT_FALSE(cottontail::splade(nullptr, queries, &results));
  EXPECT_FALSE(cottontail::splade(warren, queries, nullptr));
  for (cottontail::fval value : {-1.0, std::numeric_limits<double>::infinity(),
                                std::numeric_limits<double>::quiet_NaN()})
    EXPECT_FALSE(cottontail::splade(warren, {{"q", {{"x", value}}}}, &results));
  EXPECT_FALSE(warren->started());
  ASSERT_TRUE(append(R"({"docid":"negative","splade_vector":{"bad":-1}})"));
  EXPECT_FALSE(cottontail::splade(warren, {{"q", {{"bad", 1}}}}, &results, &error));
  EXPECT_NE(error.find("document weight"), std::string::npos);
  EXPECT_TRUE(results.empty());
  EXPECT_FALSE(warren->started());
  ASSERT_TRUE(append(R"({"docid":"large","splade_vector":{"large":1e308}})"));
  EXPECT_FALSE(cottontail::splade(warren, {{"q", {{"large", 1e308}}}},
                                 &results, &error));
  EXPECT_NE(error.find("overflow"), std::string::npos);
  EXPECT_TRUE(results.empty());
}

TEST_F(Splade, MatchesExhaustiveDotProducts) {
  std::vector<std::string> labels = {"x", "##n", "other", "UPPER", "a-b"};
  for (size_t t = 0; t < 30; t++)
    labels.push_back("term" + std::to_string(t));
  std::vector<std::map<std::string, double>> vectors;
  for (size_t d = 0; d < 40; d++) {
    std::map<std::string, double> weights;
    for (size_t t = 0; t < labels.size(); t++)
      if ((d + t) % 3 != 0)
        weights[labels[t]] = ((d * (t + 1) + t) % 13) * 0.25;
    vectors.push_back(weights);
    // Rotate physical field order without changing the exhaustive oracle.
    std::string record = "{\"docid\":\"" + std::to_string(d) +
                         "\",\"splade_vector\":{";
    bool first = true;
    for (size_t t = 0; t < labels.size(); t++) {
      auto it = weights.find(labels[(t + d) % labels.size()]);
      if (it != weights.end()) {
        if (!first)
          record += ",";
        first = false;
        record += json(it->first).dump() + ":" + json(it->second).dump();
      }
    }
    ASSERT_TRUE(append(record + "}}")) << error;
  }
  Queries queries;
  for (size_t n = 0; n < 9; n++)
    for (size_t t = 0; t < labels.size(); t++)
      queries[std::to_string(n)][labels[t]] = ((n + 1) * (t + 2) % 7) * 0.5;
  for (size_t depth : {1, 7, 1000}) {
    Results expected;
    for (const auto &query : queries) {
      std::vector<std::pair<double, size_t>> scores;
      for (size_t d = 0; d < vectors.size(); d++) {
        double score = 0;
        for (const auto &term : query.second) {
          auto it = vectors[d].find(term.first);
          if (it != vectors[d].end())
            score += term.second * it->second;
        }
        if (score > 0)
          scores.emplace_back(score, d);
      }
      std::sort(scores.begin(), scores.end(), [](const auto &a, const auto &b) {
        return a.first != b.first ? a.first > b.first : a.second < b.second;
      });
      auto &ids = expected[query.first];
      for (size_t r = 0; r < std::min(depth, scores.size()); r++)
        ids.push_back(std::to_string(scores[r].second));
    }
    Results results;
    ASSERT_TRUE(cottontail::splade(warren, queries,
                                  {{"depth", std::to_string(depth)}},
                                  &results, &error, 4)) << error;
    EXPECT_EQ(identifiers(results), expected);
  }
}

} // namespace
