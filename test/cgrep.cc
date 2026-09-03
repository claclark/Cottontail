#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gtest/gtest.h"

#include "regexp/cgrep.h"
#include "regexp/haystack.h"
#include "regexp/nfa.h"

namespace {

class StringHaystack final : public cottontail::regexp::Haystack {
public:
  StringHaystack(std::string text, std::vector<std::size_t> ends = {},
                 bool replayable = true, std::size_t fail_at = no_failure)
      : text_(std::move(text)), ends_(std::move(ends)), replayable_(replayable),
        fail_at_(fail_at) {
    if (ends_.empty() || ends_.back() < text_.size())
      ends_.push_back(text_.size());
  }

  bool chunk(const char **start, const char **end) final {
    touched_ = true;
    if (chunk_ == fail_at_) {
      error_ = "Injected Haystack failure";
      return false;
    }
    while (chunk_ < ends_.size()) {
      std::size_t next = std::min(ends_[chunk_++], text_.size());
      if (next <= offset_)
        continue;
      *start = text_.data() + offset_;
      *end = text_.data() + next;
      offset_ = next;
      return true;
    }
    return false;
  }

  std::string translate(cottontail::addr p, cottontail::addr q) final {
    const char *start;
    const char *end;
    if (!translate(p, q, &start, &end))
      return "";
    return std::string(start, end);
  }

  bool translate(cottontail::addr p, cottontail::addr q, const char **start,
                 const char **end) final {
    if (p < 0 || q < p || static_cast<std::size_t>(q) >= text_.size()) {
      error_ = "Translation outside StringHaystack";
      return false;
    }
    *start = text_.data() + p;
    *end = text_.data() + q + 1;
    return true;
  }

  void limit(cottontail::addr x) final {
    limit_ = std::max(limit_, x);
    limits_.push_back(x);
  }

  bool reset(std::string *error) final {
    if (touched_ && !replayable_) {
      cottontail::safe_error_helper(error, __FILE__, __LINE__) =
          "StringHaystack is not replayable";
      return false;
    }
    offset_ = 0;
    chunk_ = 0;
    limit_ = -1;
    limits_.clear();
    touched_ = false;
    error_.clear();
    return true;
  }

  bool success(std::string *error) final {
    if (error_.empty())
      return true;
    cottontail::safe_error_helper(error, __FILE__, __LINE__) = error_;
    return false;
  }

  cottontail::addr limit() const { return limit_; }
  const std::vector<cottontail::addr> &limits() const { return limits_; }

  static constexpr std::size_t no_failure =
      std::numeric_limits<std::size_t>::max();

private:
  std::string text_;
  std::vector<std::size_t> ends_;
  std::string error_;
  std::size_t offset_ = 0;
  std::size_t chunk_ = 0;
  cottontail::addr limit_ = -1;
  std::vector<cottontail::addr> limits_;
  bool touched_ = false;
  bool replayable_;
  std::size_t fail_at_;
};

std::vector<std::pair<std::size_t, std::size_t>>
matches(const std::string &expression, const std::string &text,
        const std::vector<std::size_t> &ends = {}) {
  std::shared_ptr<StringHaystack> haystack =
      std::make_shared<StringHaystack>(text, ends);
  std::string error;
  std::shared_ptr<cottontail::regexp::Cgrep> matcher =
      cottontail::regexp::Cgrep::make(expression, haystack, &error);
  EXPECT_NE(matcher, nullptr) << error;
  if (matcher == nullptr)
    return {};
  std::vector<std::pair<std::size_t, std::size_t>> answer;
  cottontail::addr p;
  cottontail::addr q;
  while (matcher->match(&p, &q))
    answer.emplace_back(static_cast<std::size_t>(p),
                        static_cast<std::size_t>(q));
  EXPECT_TRUE(matcher->success(&error)) << error;
  return answer;
}

void expect_reference(const std::string &expression, const std::string &text,
                      const std::vector<std::size_t> &ends = {}) {
  std::string error;
  std::vector<cottontail::regexp::transition> machine =
      cottontail::regexp::nfa(expression, &error);
  ASSERT_FALSE(machine.empty()) << error;
  EXPECT_EQ(matches(expression, text, ends),
            cottontail::regexp::match(machine, text));
}

struct LineAnswer {
  cottontail::regexp::LineCgrep::Match match;
  std::string lines;
};

std::vector<LineAnswer>
line_matches(const std::string &expression, const std::string &text,
             std::size_t limit, const std::vector<std::size_t> &ends = {}) {
  std::shared_ptr<StringHaystack> haystack =
      std::make_shared<StringHaystack>(text, ends);
  std::string error;
  std::shared_ptr<cottontail::regexp::LineCgrep> matcher =
      cottontail::regexp::LineCgrep::make(expression, haystack, limit, &error);
  EXPECT_NE(matcher, nullptr) << error;
  if (matcher == nullptr)
    return {};
  std::vector<LineAnswer> answer;
  cottontail::regexp::LineCgrep::Match match;
  while (matcher->match(&match)) {
    std::string lines;
    if (match.has_lines)
      lines = matcher->translate(match);
    answer.push_back(LineAnswer{match, lines});
  }
  EXPECT_TRUE(matcher->success(&error)) << error;
  return answer;
}

} // namespace

TEST(CgrepTest, MatchesReferenceMachine) {
  expect_reference("ab|b", "ab");
  expect_reference("a+", "aaa");
  expect_reference("^.*b|a.*b", "xaaaaab");
  expect_reference(" cat ", " cat cat cat ");
  expect_reference("a+&aa", "aaa");
  expect_reference(
      "\\n.*\\n&.*[Tt]he [Tt]he.*",
      "preface\nThe the first\nordinary\nthe The second\nepilogue\n");
}

TEST(CgrepTest, IgnoresEveryChunkBoundary) {
  std::string text = "zero café\r\n中国 🤖\nlast";
  std::vector<std::string> expressions = {"café\\R中国", "中国 🤖", "^zero",
                                          "last$",       ".+",          "\\R"};
  std::vector<std::size_t> one_byte;
  for (std::size_t i = 1; i <= text.size(); i++)
    one_byte.push_back(i);
  for (const std::string &expression : expressions) {
    expect_reference(expression, text, one_byte);
    for (std::size_t split = 1; split < text.size(); split++)
      expect_reference(expression, text, {split, text.size()});
  }
}

TEST(CgrepTest, MatchesNulAndUnterminatedInput) {
  std::string text("a\0b", 3);
  expect_reference("a\\x00b$", text, {1, 2, 3});
  EXPECT_EQ(matches("\\x00", text),
            (std::vector<std::pair<std::size_t, std::size_t>>{{1, 1}}));
}

TEST(CgrepTest, HandlesEmptyInputAndVirtualOnlyMatches) {
  expect_reference("a", "");
  expect_reference("^$", "");
  expect_reference("^", "text");
  expect_reference("$", "text");
}

TEST(CgrepTest, SharesImmutableMachine) {
  std::string error;
  std::shared_ptr<const cottontail::regexp::Cgrep::Machine> machine =
      cottontail::regexp::Cgrep::compile("cat", &error);
  ASSERT_NE(machine, nullptr) << error;
  auto first = cottontail::regexp::Cgrep::make(
      machine, std::make_shared<StringHaystack>("cat"), &error);
  auto second = cottontail::regexp::Cgrep::make(
      machine, std::make_shared<StringHaystack>("a cat"), &error);
  ASSERT_NE(first, nullptr) << error;
  ASSERT_NE(second, nullptr) << error;
  cottontail::addr p;
  cottontail::addr q;
  EXPECT_TRUE(first->match(&p, &q));
  EXPECT_EQ(std::make_pair(p, q),
            (std::pair<cottontail::addr, cottontail::addr>{0, 2}));
  EXPECT_TRUE(second->match(&p, &q));
  EXPECT_EQ(std::make_pair(p, q),
            (std::pair<cottontail::addr, cottontail::addr>{2, 4}));
}

TEST(CgrepTest, TranslatesAndResets) {
  std::shared_ptr<StringHaystack> haystack = std::make_shared<StringHaystack>(
      "a cat b", std::vector<std::size_t>{3, 7});
  std::string error;
  std::shared_ptr<cottontail::regexp::Cgrep> matcher =
      cottontail::regexp::Cgrep::make("cat", haystack, &error);
  ASSERT_NE(matcher, nullptr) << error;
  cottontail::addr p;
  cottontail::addr q;
  ASSERT_TRUE(matcher->match(&p, &q));
  EXPECT_EQ(matcher->translate(p, q), "cat");
  const char *start;
  const char *end;
  ASSERT_TRUE(matcher->translate(p, q, &start, &end));
  EXPECT_EQ(std::string(start, end), "cat");
  EXPECT_FALSE(matcher->match(&p, &q));
  EXPECT_EQ(haystack->limit(), 6);
  ASSERT_TRUE(matcher->reset(&error)) << error;
  EXPECT_TRUE(matcher->match(&p, &q));
  EXPECT_EQ(std::make_pair(p, q),
            (std::pair<cottontail::addr, cottontail::addr>{2, 4}));
}

TEST(CgrepTest, AdvancesRawLimitWithoutActiveStates) {
  std::shared_ptr<StringHaystack> haystack = std::make_shared<StringHaystack>(
      "abcdef", std::vector<std::size_t>{2, 6});
  std::string error;
  std::shared_ptr<cottontail::regexp::Cgrep> matcher =
      cottontail::regexp::Cgrep::make("z", haystack, &error);
  ASSERT_NE(matcher, nullptr) << error;
  cottontail::addr p;
  cottontail::addr q;
  EXPECT_FALSE(matcher->match(&p, &q));
  EXPECT_TRUE(matcher->success(&error)) << error;
  EXPECT_EQ(haystack->limit(), 5);
  EXPECT_EQ(haystack->limits(),
            (std::vector<cottontail::addr>{1, 5}));
}

TEST(CgrepTest, AdvancesLimitWhenActiveStatesDisappear) {
  std::shared_ptr<StringHaystack> haystack =
      std::make_shared<StringHaystack>("abbbbb");
  std::string error;
  std::shared_ptr<cottontail::regexp::Cgrep> matcher =
      cottontail::regexp::Cgrep::make("az", haystack, &error);
  ASSERT_NE(matcher, nullptr) << error;
  cottontail::addr p;
  cottontail::addr q;
  EXPECT_FALSE(matcher->match(&p, &q));
  EXPECT_TRUE(matcher->success(&error)) << error;
  EXPECT_EQ(haystack->limits(),
            (std::vector<cottontail::addr>{1, 5}));
}

TEST(CgrepTest, ReportsCompleteLinesAndQueuesSameLineMatches) {
  std::string text = "zero\ncat cat\nwrap\nped\n";
  std::vector<LineAnswer> cats = line_matches("cat", text, 4);
  ASSERT_EQ(cats.size(), 2u);
  EXPECT_EQ(cats[0].lines, "cat cat\n");
  EXPECT_EQ(cats[1].lines, "cat cat\n");
  EXPECT_EQ(std::make_pair(cats[0].match.p, cats[0].match.q),
            (std::pair<cottontail::addr, cottontail::addr>{5, 7}));
  EXPECT_EQ(cats[0].match.lines_p, 5);
  EXPECT_EQ(cats[0].match.lines_q, 12);
  EXPECT_EQ(cats[0].match.start_line, 2u);
  EXPECT_EQ(cats[0].match.start_position, 1u);
  EXPECT_EQ(cats[0].match.end_line, 2u);
  EXPECT_EQ(cats[0].match.end_position, 3u);
  EXPECT_EQ(cats[1].match.start_position, 5u);
  EXPECT_EQ(cats[1].match.end_position, 7u);

  std::vector<LineAnswer> wrapped = line_matches("wrap\\nped", text, 2);
  ASSERT_EQ(wrapped.size(), 1u);
  EXPECT_TRUE(wrapped[0].match.has_lines);
  EXPECT_EQ(wrapped[0].lines, "wrap\nped\n");
  EXPECT_EQ(wrapped[0].match.start_line, 3u);
  EXPECT_EQ(wrapped[0].match.start_position, 1u);
  EXPECT_EQ(wrapped[0].match.end_line, 4u);
  EXPECT_EQ(wrapped[0].match.end_position, 3u);
}

TEST(CgrepTest, AppliesLineLimitAndUnlimitedMode) {
  std::string text = "one\ntwo\nthree\n";
  std::vector<LineAnswer> limited = line_matches("one\\ntwo", text, 1);
  ASSERT_EQ(limited.size(), 1u);
  EXPECT_FALSE(limited[0].match.has_lines);
  EXPECT_EQ(limited[0].lines, "");

  std::vector<LineAnswer> unlimited = line_matches("^.*$", text, 0);
  ASSERT_EQ(unlimited.size(), 1u);
  EXPECT_TRUE(unlimited[0].match.has_lines);
  EXPECT_EQ(unlimited[0].lines, text);
  EXPECT_EQ(unlimited[0].match.start_line, 1u);
  EXPECT_EQ(unlimited[0].match.end_line, 3u);
}

TEST(CgrepTest, HandlesLineChunkBoundariesAndEof) {
  std::string text = "café\n中国 🤖";
  std::vector<std::size_t> one_byte;
  for (std::size_t i = 1; i <= text.size(); i++)
    one_byte.push_back(i);
  std::vector<LineAnswer> answer =
      line_matches("中国 🤖$", text, 4, one_byte);
  ASSERT_EQ(answer.size(), 1u);
  EXPECT_TRUE(answer[0].match.has_lines);
  EXPECT_EQ(answer[0].lines, "中国 🤖");
  EXPECT_EQ(answer[0].match.start_line, 2u);
  EXPECT_EQ(answer[0].match.start_position, 1u);
  EXPECT_EQ(answer[0].match.end_line, 2u);
  EXPECT_EQ(answer[0].match.end_position, 11u);
}

TEST(CgrepTest, AdvancesLineLimitWithoutActiveStates) {
  std::shared_ptr<StringHaystack> haystack = std::make_shared<StringHaystack>(
      "first\nsecond", std::vector<std::size_t>{3, 8, 12});
  std::string error;
  std::shared_ptr<cottontail::regexp::LineCgrep> matcher =
      cottontail::regexp::LineCgrep::make("z", haystack, 4, &error);
  ASSERT_NE(matcher, nullptr) << error;
  cottontail::regexp::LineCgrep::Match match;
  EXPECT_FALSE(matcher->match(&match));
  EXPECT_TRUE(matcher->success(&error)) << error;
  EXPECT_EQ(haystack->limit(), 5);
  EXPECT_EQ(haystack->limits(),
            (std::vector<cottontail::addr>{5}));
}

TEST(CgrepTest, RejectsResetOfConsumedOneShotInput) {
  std::string error = "untouched";
  std::shared_ptr<StringHaystack> haystack = std::make_shared<StringHaystack>(
      "cat", std::vector<std::size_t>{}, false);
  EXPECT_TRUE(haystack->reset(&error));
  EXPECT_EQ(error, "untouched");
  std::shared_ptr<cottontail::regexp::Cgrep> matcher =
      cottontail::regexp::Cgrep::make("cat", haystack, &error);
  ASSERT_NE(matcher, nullptr);
  cottontail::addr p;
  cottontail::addr q;
  EXPECT_TRUE(matcher->match(&p, &q));
  EXPECT_FALSE(matcher->reset(&error));
  EXPECT_NE(error.find("not replayable"), std::string::npos);
}

TEST(CgrepTest, PropagatesHaystackFailure) {
  std::shared_ptr<StringHaystack> haystack = std::make_shared<StringHaystack>(
      "abc", std::vector<std::size_t>{1, 3}, true, 1);
  std::string error;
  std::shared_ptr<cottontail::regexp::Cgrep> matcher =
      cottontail::regexp::Cgrep::make("z", haystack, &error);
  ASSERT_NE(matcher, nullptr) << error;
  cottontail::addr p;
  cottontail::addr q;
  EXPECT_FALSE(matcher->match(&p, &q));
  EXPECT_FALSE(matcher->success(&error));
  EXPECT_NE(error.find("Injected Haystack failure"), std::string::npos);
}

TEST(CgrepTest, RejectsInvalidTranslation) {
  std::string error;
  std::shared_ptr<cottontail::regexp::Cgrep> matcher =
      cottontail::regexp::Cgrep::make(
          "a", std::make_shared<StringHaystack>("a"), &error);
  ASSERT_NE(matcher, nullptr) << error;
  cottontail::addr p;
  cottontail::addr q;
  ASSERT_TRUE(matcher->match(&p, &q));
  EXPECT_EQ(matcher->translate(0, 1), "");
  EXPECT_FALSE(matcher->success(&error));
  EXPECT_NE(error.find("Translation outside"), std::string::npos);
}

TEST(CgrepTest, RejectsInvalidPublicMachine) {
  std::string error;
  cottontail::regexp::transition invalid{
      cottontail::regexp::final_state, cottontail::regexp::start_state, {'x'}};
  EXPECT_EQ(cottontail::regexp::Cgrep::compile({invalid}, &error), nullptr);
  EXPECT_NE(error.find("final state"), std::string::npos);
}
