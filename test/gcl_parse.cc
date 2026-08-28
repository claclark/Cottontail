#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gtest/gtest.h"

#include "gcl/parse.h"
#include "src/featurizer.h"
#include "src/hashing_featurizer.h"
#include "src/null_idx.h"
#include "src/tokenizer.h"

namespace {

std::shared_ptr<cottontail::gcl::SExpression> parse(const std::string &gcl) {
  std::string error;
  std::shared_ptr<cottontail::gcl::SExpression> expr =
      cottontail::gcl::SExpression::from_string(gcl, &error);
  EXPECT_NE(expr, nullptr) << error;
  return expr;
}

std::string canonical(const std::string &gcl) {
  std::shared_ptr<cottontail::gcl::SExpression> expr = parse(gcl);
  if (expr == nullptr)
    return "";
  return expr->to_string();
}

void expect_invalid(const std::string &gcl) {
  std::string error;
  EXPECT_EQ(cottontail::gcl::SExpression::from_string(gcl, &error), nullptr);
}

class RecordingTokenizer final : public cottontail::Tokenizer {
public:
  explicit RecordingTokenizer(std::vector<std::string> terms)
      : terms_(std::move(terms)){};
  virtual ~RecordingTokenizer(){};
  RecordingTokenizer(const RecordingTokenizer &) = delete;
  RecordingTokenizer &operator=(const RecordingTokenizer &) = delete;
  RecordingTokenizer(RecordingTokenizer &&) = delete;
  RecordingTokenizer &operator=(RecordingTokenizer &&) = delete;

  std::string seen() const { return seen_; }

private:
  std::string recipe_() final { return ""; }
  std::vector<cottontail::Token>
  tokenize_(std::shared_ptr<cottontail::Featurizer> featurizer, char *buffer,
            size_t length) final {
    (void)featurizer;
    (void)buffer;
    (void)length;
    return {};
  }
  const char *skip_(const char *buffer, size_t length, cottontail::addr n) final {
    (void)n;
    return buffer + length;
  }
  std::vector<std::string> split_(const std::string &text) final {
    (void)text;
    return {};
  }
  std::vector<std::string> phrase_(const std::string &text) final {
    seen_ = text;
    return terms_;
  }
  bool destructive_() final { return false; }

  std::vector<std::string> terms_;
  std::string seen_;
};

} // namespace

TEST(GCLParseTest, CanonicalLiteralTerms) {
  EXPECT_EQ(canonical("|foo|"), "foo");
  EXPECT_EQ(canonical("|foo bar|"), "|foo bar|");
  EXPECT_EQ(canonical("||"), "||");
  EXPECT_EQ(canonical("|(foo)|"), "|(foo)|");
  EXPECT_EQ(canonical("|\"foo bar\"|"), "|\"foo bar\"|");
  EXPECT_EQ(canonical("|'foo'|"), "|'foo'|");
  EXPECT_EQ(canonical("|`foo`|"), "|`foo`|");
  EXPECT_EQ(canonical(R"gcl(|foo\|bar|)gcl"), "foo|bar");
  EXPECT_EQ(canonical(R"gcl(|\|foo|)gcl"), R"gcl(|\|foo|)gcl");
}

TEST(GCLParseTest, LiteralEscapes) {
  EXPECT_EQ(canonical(R"gcl(|\a\b\f\n\r\t\v|)gcl"),
            R"gcl(|\a\b\f\n\r\t\v|)gcl");
  EXPECT_EQ(canonical(R"gcl(|\x0a|)gcl"), R"gcl(|\n|)gcl");
  EXPECT_EQ(canonical(R"gcl(|\x41\u03a9\U0001f916|)gcl"), "AΩ🤖");
  EXPECT_EQ(canonical(R"gcl(|\q\xq\x4\u123\U1234567|)gcl"),
            "qxqx4u123U1234567");
}

TEST(GCLParseTest, InvalidLiteralTerms) {
  expect_invalid("|foo");
  expect_invalid("|foo\\");
  expect_invalid("|foo\nbar|");
  expect_invalid(R"gcl(|\x00|)gcl");
  expect_invalid(R"gcl(|\u0000|)gcl");
  expect_invalid(R"gcl(|\U00000000|)gcl");
  expect_invalid(R"gcl(|\uD800|)gcl");
  expect_invalid(R"gcl(|\U00110000|)gcl");
}

TEST(GCLParseTest, InvalidFixedWidths) {
  std::string error;
  EXPECT_EQ(cottontail::gcl::SExpression::from_string("(# 0 )", &error),
            nullptr);
  EXPECT_EQ(error.find("parse error at offset 4:(# 0 )"), (size_t)0);
}

TEST(GCLParseTest, QuoteKindsAndBackslashParity) {
  EXPECT_EQ(canonical("\"foo bar\""), "\"foo bar\"");
  EXPECT_EQ(canonical("'foo bar'"), "'foo bar'");
  EXPECT_EQ(canonical("`foo bar`"), "`foo bar`");
  EXPECT_EQ(canonical(R"gcl("foo\\")gcl"), R"gcl("foo\\")gcl");
  EXPECT_EQ(canonical(R"gcl("foo\"bar")gcl"), R"gcl("foo\"bar")gcl");

  std::shared_ptr<cottontail::Featurizer> featurizer =
      cottontail::HashingFeaturizer::make();
  std::shared_ptr<cottontail::Idx> idx = cottontail::NullIdx::make("");
  EXPECT_EQ(parse("'foo'")->to_hopper(featurizer, idx), nullptr);
  EXPECT_NE(parse("|'foo'|")->to_hopper(featurizer, idx), nullptr);
}

TEST(GCLParseTest, FixedWidthOneCompilesToUniversalHopper) {
  std::shared_ptr<cottontail::Featurizer> featurizer =
      cottontail::HashingFeaturizer::make();
  std::shared_ptr<cottontail::Idx> idx = cottontail::NullIdx::make("");
  std::unique_ptr<cottontail::Hopper> one =
      parse("(# 1)")->to_hopper(featurizer, idx);
  std::unique_ptr<cottontail::Hopper> two =
      parse("(# 2)")->to_hopper(featurizer, idx);
  ASSERT_NE(one, nullptr);
  ASSERT_NE(two, nullptr);
  EXPECT_NE(dynamic_cast<cottontail::UniversalHopper *>(one.get()), nullptr);
  EXPECT_NE(dynamic_cast<cottontail::FixedWidthHopper *>(two.get()), nullptr);
}

TEST(GCLParseTest, PhraseExpansionEscapesAndSerializesTerms) {
  std::shared_ptr<RecordingTokenizer> tokenizer =
      std::make_shared<RecordingTokenizer>(
          std::vector<std::string>{"foo bar", "(qux)"});
  std::shared_ptr<cottontail::gcl::SExpression> expr =
      parse(R"gcl("a\\b\"c")gcl");
  expr = expr->expand_phrases(tokenizer);
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(tokenizer->seen(), "a\\b\"c");
  EXPECT_EQ(expr->to_string(), "(>> (# 2) (... |foo bar| |(qux)|))");

  tokenizer = std::make_shared<RecordingTokenizer>(
      std::vector<std::string>{"foo bar"});
  expr = parse("\"ignored\"")->expand_phrases(tokenizer);
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->to_string(), "|foo bar|");
}
