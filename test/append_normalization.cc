#include <memory>
#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "src/appender.h"
#include "src/builder.h"
#include "src/core.h"
#include "src/featurizer.h"

namespace {

class RecordingAppender final : public cottontail::Appender {
public:
  RecordingAppender() = default;
  std::string text;

private:
  std::string recipe_() final { return ""; }
  bool append_(const std::string &input, cottontail::addr *p,
               cottontail::addr *q, std::string *error) final {
    text = input;
    *p = 0;
    *q = -1;
    return true;
  }
};

class RecordingBuilder final : public cottontail::Builder {
public:
  RecordingBuilder() = default;
  std::string text;

private:
  std::shared_ptr<cottontail::Featurizer> get_featurizer_() final {
    return nullptr;
  }
  bool add_text_(const std::string &input, cottontail::addr *p,
                 cottontail::addr *q, std::string *error) final {
    text = input;
    *p = 0;
    *q = -1;
    return true;
  }
  bool add_annotation_(const std::string &tag, cottontail::addr p,
                       cottontail::addr q, cottontail::fval v,
                       std::string *error) final {
    return true;
  }
  bool add_annotation_(cottontail::addr feature, cottontail::addr p,
                       cottontail::addr q, cottontail::fval v,
                       std::string *error) final {
    return true;
  }
  bool finalize_(std::string *error) final { return true; }
};

struct NormalizationCase {
  std::string input;
  std::string expected;
};

const std::vector<NormalizationCase> cases = {
    {"", ""},         {"x", "x\n"},     {"x ", "x "},
    {"x\t", "x\t"},   {"x\r", "x\r"},   {"x\n", "x\n"},
    {"x\v", "x\v\n"}, {"x\f", "x\f\n"},
};

} // namespace

TEST(AppendNormalization, Appender) {
  RecordingAppender appender;
  for (const auto &item : cases) {
    cottontail::addr p, q;
    ASSERT_TRUE(appender.append(item.input, &p, &q));
    EXPECT_EQ(appender.text, item.expected);
  }
}

TEST(AppendNormalization, Builder) {
  RecordingBuilder builder;
  for (const auto &item : cases) {
    cottontail::addr p, q;
    ASSERT_TRUE(builder.add_text(item.input, &p, &q));
    EXPECT_EQ(builder.text, item.expected);
  }
}
