#include <algorithm>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "gtest/gtest.h"

#include "regexp/buffer_cgrep.h"
#include "regexp/cgrep.h"
#include "regexp/cgrep_internal.h"
#include "regexp/haystack.h"
#include "regexp/haystack_cgrep.h"
#include "regexp/nfa.h"

namespace {

class StringHaystack final : public cottontail::regexp::Haystack {
public:
  StringHaystack(std::string text, std::vector<std::size_t> ends = {},
                 bool replayable = true, std::size_t fail_at = no_failure,
                 bool relocate = false)
      : text_(std::move(text)), ends_(std::move(ends)), replayable_(replayable),
        fail_at_(fail_at), relocate_(relocate) {
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
      if (relocate_) {
        base_ = static_cast<std::size_t>(limit_ + 1);
        std::vector<char> window(text_.begin() + base_, text_.begin() + next);
        window_.swap(window);
        *start = window_.data() + (offset_ - base_);
        *end = window_.data() + window_.size();
      } else {
        *start = text_.data() + offset_;
        *end = text_.data() + next;
      }
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
    if (relocate_) {
      if (p <= limit_ || static_cast<std::size_t>(q) >= offset_) {
        error_ = "Translation outside retained StringHaystack";
        return false;
      }
      *start = window_.data() + (p - static_cast<cottontail::addr>(base_));
      *end = window_.data() + (q - static_cast<cottontail::addr>(base_)) + 1;
    } else {
      *start = text_.data() + p;
      *end = text_.data() + q + 1;
    }
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
    window_.clear();
    base_ = 0;
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
  bool relocate_;
  std::vector<char> window_;
  std::size_t base_ = 0;
};

std::vector<std::pair<std::size_t, std::size_t>>
matches(const std::string &expression, const std::string &text,
        const std::vector<std::size_t> &ends = {}, bool relocate = false,
        bool springy = true) {
  std::shared_ptr<StringHaystack> haystack = std::make_shared<StringHaystack>(
      text, ends, true, StringHaystack::no_failure, relocate);
  std::string error;
  auto machine =
      cottontail::regexp::Cgrep::compile(expression, &error, springy);
  std::shared_ptr<cottontail::regexp::Cgrep> matcher =
      cottontail::regexp::Cgrep::make(machine, haystack, &error);
  EXPECT_NE(matcher, nullptr) << error;
  if (matcher == nullptr)
    return {};
  std::vector<std::pair<std::size_t, std::size_t>> answer;
  cottontail::addr p;
  cottontail::addr q;
  while (matcher->match(&p, &q)) {
    answer.emplace_back(static_cast<std::size_t>(p),
                        static_cast<std::size_t>(q));
    if (relocate) {
      EXPECT_EQ(matcher->translate(p, q), text.substr(p, q - p + 1));
    }
  }
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

std::vector<std::pair<std::size_t, std::size_t>>
buffer_matches(const std::string &expression, const std::string &text,
               bool springy = true) {
  std::string error;
  auto machine =
      cottontail::regexp::Cgrep::compile(expression, &error, springy);
  auto matcher = cottontail::regexp::Cgrep::make(machine, text.data(),
                                                 text.size(), &error);
  EXPECT_NE(matcher, nullptr) << error;
  if (matcher == nullptr)
    return {};
  std::vector<std::pair<std::size_t, std::size_t>> answer;
  cottontail::addr p;
  cottontail::addr q;
  while (matcher->match(&p, &q)) {
    answer.emplace_back(p, q);
    EXPECT_EQ(matcher->translate(p, q), text.substr(p, q - p + 1));
  }
  EXPECT_TRUE(matcher->success(&error)) << error;
  return answer;
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

std::vector<LineAnswer> buffer_line_matches(const std::string &expression,
                                            const std::string &text,
                                            std::size_t limit) {
  auto storage = std::make_shared<const std::string>(text);
  std::shared_ptr<const char> buffer(storage, storage->data());
  std::string error;
  auto machine = cottontail::regexp::Cgrep::compile(expression, &error);
  auto matcher = cottontail::regexp::LineCgrep::make(
      machine, buffer, text.size(), limit, &error);
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

void expect_same_lines(const std::vector<LineAnswer> &actual,
                       const std::vector<LineAnswer> &expected) {
  ASSERT_EQ(actual.size(), expected.size());
  for (std::size_t i = 0; i < actual.size(); i++) {
    SCOPED_TRACE(i);
    EXPECT_EQ(actual[i].lines, expected[i].lines);
    const auto &a = actual[i].match;
    const auto &e = expected[i].match;
    EXPECT_EQ(a.p, e.p);
    EXPECT_EQ(a.q, e.q);
    EXPECT_EQ(a.lines_p, e.lines_p);
    EXPECT_EQ(a.lines_q, e.lines_q);
    EXPECT_EQ(a.start_line, e.start_line);
    EXPECT_EQ(a.start_position, e.start_position);
    EXPECT_EQ(a.end_line, e.end_line);
    EXPECT_EQ(a.end_position, e.end_position);
    EXPECT_EQ(a.has_lines, e.has_lines);
  }
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
      "a cat b", std::vector<std::size_t>{3, 7}, true,
      StringHaystack::no_failure, true);
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
  EXPECT_EQ(haystack->limits(), (std::vector<cottontail::addr>{1, 5}));
}

TEST(CgrepTest, AdvancesLimitWhenActiveStatesDisappear) {
  std::shared_ptr<StringHaystack> haystack =
      std::make_shared<StringHaystack>("abbbbb");
  std::string error;
  std::shared_ptr<cottontail::regexp::Cgrep> matcher =
      cottontail::regexp::Cgrep::make("a[yz]", haystack, &error);
  ASSERT_NE(matcher, nullptr) << error;
  cottontail::addr p;
  cottontail::addr q;
  EXPECT_FALSE(matcher->match(&p, &q));
  EXPECT_TRUE(matcher->success(&error)) << error;
  EXPECT_EQ(haystack->limits(), (std::vector<cottontail::addr>{1, 5}));
}

TEST(CgrepTest, TightensBufferLiteralCandidates) {
  std::string text = "a__ab_c__abcabc";
  std::string error;
  auto machine = cottontail::regexp::Cgrep::compile("abc", &error);
  auto matcher = cottontail::regexp::Cgrep::make(machine, text.data(),
                                                 text.size(), &error);
  ASSERT_NE(matcher, nullptr) << error;
  cottontail::addr p;
  cottontail::addr q;
  ASSERT_TRUE(matcher->match(&p, &q));
  EXPECT_EQ(p, 9);
  EXPECT_EQ(q, 11);
  EXPECT_EQ(matcher->translate(p, q), "abc");
  ASSERT_TRUE(matcher->match(&p, &q));
  EXPECT_EQ(p, 12);
  EXPECT_EQ(q, 14);
  EXPECT_EQ(matcher->translate(p, q), "abc");
  EXPECT_FALSE(matcher->match(&p, &q));
  EXPECT_TRUE(matcher->success(&error)) << error;
}

TEST(CgrepTest, MatchesLiteralsAcrossRelocatedAndTrimmedChunks) {
  std::vector<std::pair<std::string, std::string>> cases = {
      {"abc", "a__ab_c__abcabc"},
      {"abc", "a_b_ca_b_c"},
      {"abc", "ab"},
      {"abc", "zzz"},
      {"abc", ""},
      {"aba", "ababababa"},
      {"aaa", "aaaaaaa"},
      {"a", "aaaa"},
      {"a\\x00b", std::string("a\0_ba\0b", 7)},
      {"café", "c___afé café café"},
      {"中国🤖", "中__国🤖中国🤖中国🤖"}};
  for (const auto &[expression, text] : cases) {
    SCOPED_TRACE(expression);
    std::string error;
    auto machine = cottontail::regexp::nfa(expression, &error);
    ASSERT_FALSE(machine.empty()) << error;
    auto expected = cottontail::regexp::match(machine, text);
    EXPECT_EQ(buffer_matches(expression, text), expected);
    EXPECT_EQ(buffer_matches(expression, text, false), expected);
    EXPECT_EQ(matches(expression, text, {}, true), expected);
    std::vector<std::size_t> one_byte;
    for (std::size_t i = 1; i <= text.size(); i++)
      one_byte.push_back(i);
    EXPECT_EQ(matches(expression, text, one_byte, true), expected);
    EXPECT_EQ(matches(expression, text, one_byte, true, false), expected);
    for (std::size_t split = 1; split < text.size(); split++)
      EXPECT_EQ(matches(expression, text, {split, text.size()}, true),
                expected);
  }
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
  EXPECT_EQ(haystack->limits(), (std::vector<cottontail::addr>{5}));
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

TEST(CgrepTest, BufferMatchesEveryByteValue) {
  std::string text;
  for (unsigned int byte = 0; byte < 256; byte++)
    text.push_back(static_cast<char>(byte));
  text += text;
  for (unsigned int byte = 0; byte < 256; byte++) {
    SCOPED_TRACE(byte);
    std::vector<cottontail::regexp::transition> transitions = {
        {cottontail::regexp::start_state,
         cottontail::regexp::final_state,
         {static_cast<cottontail::regexp::symbol>(byte)}}};
    std::string error;
    auto bundle = cottontail::regexp::Cgrep::compile(transitions, &error);
    ASSERT_NE(bundle, nullptr) << error;
    auto matcher = cottontail::regexp::Cgrep::make(bundle, text.data(),
                                                   text.size(), &error);
    ASSERT_NE(matcher, nullptr) << error;
    cottontail::addr p;
    cottontail::addr q;
    ASSERT_TRUE(matcher->match(&p, &q));
    EXPECT_EQ(p, byte);
    EXPECT_EQ(q, byte);
    ASSERT_TRUE(matcher->match(&p, &q));
    EXPECT_EQ(p, byte + 256);
    EXPECT_EQ(q, byte + 256);
    EXPECT_EQ(matcher->translate(p, q),
              std::string(1, static_cast<char>(byte)));
    EXPECT_FALSE(matcher->match(&p, &q));
    EXPECT_TRUE(matcher->success(&error));
  }
}

TEST(CgrepTest, BufferFallbackMatchesReference) {
  std::vector<std::pair<std::string, std::string>> cases = {
      {"a.*b", "a_a_b a_b"},
      {"ab|bc", "abcabc"},
      {"[a-c]+d", "abcd abd bcd"},
      {"^a.*b$", "a\nb"},
      {"^$", ""},
      {"^", "abc"},
      {"$", "abc"},
      {"\\n.*\\n&.*[Tt]he [Tt]he.*", "\nThe The\nthe the\n"}};
  for (const auto &[expression, text] : cases) {
    SCOPED_TRACE(expression);
    std::string error;
    auto nfa = cottontail::regexp::nfa(expression, &error);
    ASSERT_FALSE(nfa.empty()) << error;
    EXPECT_EQ(buffer_matches(expression, text),
              cottontail::regexp::match(nfa, text));
  }
}

TEST(CgrepTest, OwnsBufferThroughSpecializationAndFallback) {
  for (const std::string expression : {"cat", "c.t"}) {
    SCOPED_TRACE(expression);
    std::string error = "untouched";
    auto machine = cottontail::regexp::Cgrep::compile(expression, &error);
    auto storage = std::make_shared<const std::string>("a cat b");
    std::weak_ptr<const std::string> owner = storage;
    const char *data = storage->data();
    std::shared_ptr<const char> buffer(storage, data);
    auto matcher = cottontail::regexp::Cgrep::make(machine, buffer, 7, &error);
    ASSERT_NE(matcher, nullptr) << error;
    storage.reset();
    buffer.reset();
    EXPECT_FALSE(owner.expired());
    cottontail::addr p;
    cottontail::addr q;
    ASSERT_TRUE(matcher->match(&p, &q));
    EXPECT_EQ(p, 2);
    EXPECT_EQ(q, 4);
    const char *start;
    const char *end;
    ASSERT_TRUE(matcher->translate(p, q, &start, &end));
    EXPECT_EQ(start, data + 2);
    EXPECT_EQ(end, data + 5);
    std::string copy = matcher->translate(p, q);
    EXPECT_FALSE(matcher->match(&p, &q));
    ASSERT_TRUE(matcher->reset(&error)) << error;
    EXPECT_TRUE(matcher->match(&p, &q));
    EXPECT_TRUE(matcher->success(&error));
    EXPECT_EQ(error, "untouched");
    matcher.reset();
    EXPECT_TRUE(owner.expired());
    EXPECT_EQ(copy, "cat");
  }
}

TEST(CgrepTest, CopiesUnownedBuffersAndHandlesEmptyInput) {
  std::string error;
  auto machine = cottontail::regexp::Cgrep::compile("cat", &error);
  char text[] = "cat";
  auto matcher = cottontail::regexp::Cgrep::make(machine, text, 3, &error);
  ASSERT_NE(matcher, nullptr);
  text[0] = 'b';
  cottontail::addr p;
  cottontail::addr q;
  ASSERT_TRUE(matcher->match(&p, &q));
  EXPECT_EQ(matcher->translate(p, q), "cat");
  auto empty = cottontail::regexp::Cgrep::make(
      machine, static_cast<const char *>(nullptr), 0, &error);
  ASSERT_NE(empty, nullptr) << error;
  EXPECT_FALSE(empty->match(&p, &q));
  EXPECT_TRUE(empty->success(&error));
  EXPECT_TRUE(empty->reset(&error));
  EXPECT_FALSE(empty->match(&p, &q));
  EXPECT_EQ(cottontail::regexp::Cgrep::make(
                machine, static_cast<const char *>(nullptr), 1, &error),
            nullptr);
}

TEST(CgrepTest, BufferErrorsAreStickyAndResettable) {
  for (const std::string expression : {"cat", "c.t"}) {
    SCOPED_TRACE(expression);
    std::string error;
    auto machine = cottontail::regexp::Cgrep::compile(expression, &error);
    auto matcher = cottontail::regexp::Cgrep::make(machine, "cat", 3, &error);
    ASSERT_NE(matcher, nullptr);
    cottontail::addr p;
    cottontail::addr q;
    ASSERT_TRUE(matcher->match(&p, &q));
    EXPECT_EQ(matcher->translate(-1, 2), "");
    EXPECT_FALSE(matcher->success(&error));
    EXPECT_FALSE(matcher->match(&p, &q));
    ASSERT_TRUE(matcher->reset());
    EXPECT_TRUE(matcher->match(&p, &q));
    const char *start;
    EXPECT_FALSE(matcher->translate(0, 2, &start, nullptr));
    EXPECT_FALSE(matcher->success());
    ASSERT_TRUE(matcher->reset());
    EXPECT_FALSE(matcher->match(nullptr, &q));
    EXPECT_FALSE(matcher->success());
    ASSERT_TRUE(matcher->reset());
    EXPECT_TRUE(matcher->match(&p, &q));
  }
}

TEST(CgrepTest, StreamFactoryRetainsOneShotSource) {
  std::string error = "untouched";
  auto machine = cottontail::regexp::Cgrep::compile("cat", &error);
  auto input = std::make_shared<std::istringstream>("cat cat");
  std::weak_ptr<std::istream> owner = input;
  auto matcher = cottontail::regexp::Cgrep::make(machine, input, &error);
  ASSERT_NE(matcher, nullptr) << error;
  EXPECT_NE(dynamic_cast<cottontail::regexp::HaystackCgrep *>(matcher.get()),
            nullptr);
  input.reset();
  EXPECT_FALSE(owner.expired());
  EXPECT_TRUE(matcher->reset(&error));
  EXPECT_EQ(error, "untouched");
  cottontail::addr p;
  cottontail::addr q;
  ASSERT_TRUE(matcher->match(&p, &q));
  EXPECT_EQ(matcher->translate(p, q), "cat");
  EXPECT_FALSE(matcher->reset(&error));
  EXPECT_NE(error.find("one-shot"), std::string::npos);
  ASSERT_TRUE(matcher->match(&p, &q));
  EXPECT_EQ(p, 4);
  EXPECT_EQ(q, 6);
  EXPECT_FALSE(matcher->match(&p, &q));
  EXPECT_TRUE(matcher->success());
  matcher.reset();
  EXPECT_TRUE(owner.expired());
}

TEST(CgrepTest, CompilesOnlyRequiredMachinesAndReusesThem) {
  std::string error;
  auto bundle = cottontail::regexp::Cgrep::compile("cat", &error);
  ASSERT_NE(bundle, nullptr);
  EXPECT_EQ(bundle->buffer, nullptr);
  EXPECT_EQ(bundle->haystack, nullptr);
  auto first = cottontail::regexp::Cgrep::make(bundle, "cat", 3, &error);
  ASSERT_NE(first, nullptr) << error;
  EXPECT_NE(dynamic_cast<cottontail::regexp::BufferCgrep *>(first.get()),
            nullptr);
  EXPECT_NE(bundle->buffer, nullptr);
  EXPECT_EQ(bundle->haystack, nullptr);
  auto compiled_buffer = bundle->buffer;
  auto second = cottontail::regexp::Cgrep::make(bundle, "cat", 3, &error);
  EXPECT_EQ(bundle->buffer, compiled_buffer);
  auto raw = cottontail::regexp::Cgrep::make(
      bundle, std::make_shared<StringHaystack>("cat"), &error);
  ASSERT_NE(raw, nullptr);
  auto compiled_haystack = bundle->haystack;
  EXPECT_NE(compiled_haystack, nullptr);
  auto lines = cottontail::regexp::LineCgrep::make(
      bundle, std::make_shared<StringHaystack>("cat"), 4, &error);
  ASSERT_NE(lines, nullptr);
  EXPECT_EQ(bundle->haystack, compiled_haystack);
  EXPECT_EQ(bundle->buffer, compiled_buffer);

  auto disabled = cottontail::regexp::Cgrep::compile("cat", &error, false);
  auto fallback = cottontail::regexp::Cgrep::make(disabled, "cat", 3, &error);
  ASSERT_NE(fallback, nullptr);
  EXPECT_NE(dynamic_cast<cottontail::regexp::HaystackCgrep *>(fallback.get()),
            nullptr);
  EXPECT_NE(disabled->buffer, nullptr);
  EXPECT_NE(disabled->haystack, nullptr);
}

TEST(CgrepTest, ConcurrentLazyCompilationAndIndependentRunners) {
  for (const std::string expression : {"cat", "c.t"}) {
    SCOPED_TRACE(expression);
    std::string error;
    auto bundle = cottontail::regexp::Cgrep::compile(expression, &error);
    ASSERT_NE(bundle, nullptr);
    std::vector<std::thread> workers;
    for (int i = 0; i < 12; i++)
      workers.emplace_back([bundle, i]() {
        std::string error = "untouched";
        if (i % 3 == 2) {
          auto matcher = cottontail::regexp::LineCgrep::make(
              bundle, std::make_shared<StringHaystack>("a cat\n"), 4, &error);
          ASSERT_NE(matcher, nullptr) << error;
          cottontail::regexp::LineCgrep::Match result;
          ASSERT_TRUE(matcher->match(&result));
          EXPECT_EQ(result.p, 2);
          EXPECT_EQ(result.q, 4);
          EXPECT_EQ(matcher->translate(result), "a cat\n");
          EXPECT_FALSE(matcher->match(&result));
          EXPECT_TRUE(matcher->success(&error));
        } else {
          auto matcher =
              i % 3 == 0
                  ? cottontail::regexp::Cgrep::make(bundle, "a cat", 5, &error)
                  : cottontail::regexp::Cgrep::make(
                        bundle, std::make_shared<StringHaystack>("a cat"),
                        &error);
          ASSERT_NE(matcher, nullptr) << error;
          cottontail::addr p;
          cottontail::addr q;
          ASSERT_TRUE(matcher->match(&p, &q));
          EXPECT_EQ(p, 2);
          EXPECT_EQ(q, 4);
          EXPECT_EQ(matcher->translate(p, q), "cat");
          EXPECT_FALSE(matcher->match(&p, &q));
          EXPECT_TRUE(matcher->success(&error));
        }
        EXPECT_EQ(error, "untouched");
      });
    for (auto &worker : workers)
      worker.join();
    EXPECT_NE(bundle->buffer, nullptr);
    EXPECT_NE(bundle->haystack, nullptr);
  }
}

TEST(CgrepTest, BufferLinesAgreeWithStreamingLines) {
  std::vector<std::pair<std::string, std::string>> cases = {
      {"cat", ""},
      {"cat", "cat"},
      {"cat", "cat\ncat"},
      {"cat", "zero\ncat cat\n\ncat\n"},
      {"aba", "abababa\nabababa"},
      {"\\n", "\n\n\n"},
      {"\\n\\n", "\n\n\n\n"},
      {"cat\\n", "cat\ncat\n"},
      {"\\ncat", "\ncat\ncat"},
      {"a\\r\\nb", "a\r\nb a\r\nb\r\n"},
      {"aba\\naba", "aba\naba\naba\naba"},
      {"café\\n中国🤖", "zero\ncafé\n中国🤖 café\n中国🤖"},
      {"a\\x00b", std::string("a\0b\na\0b", 7)},
      {"cat", std::string(8192, 'x') + "cat cat\ncat"},
      {"a\\nb\\nc", "a\nb\nc\na\nb\nc"},
      {"c.t", "cat\ncot"},
      {"^.*$", "one\ntwo\nthree"}};
  for (const auto &[expression, text] : cases)
    for (std::size_t limit : {0u, 1u, 2u, 4u}) {
      SCOPED_TRACE(expression);
      SCOPED_TRACE(limit);
      auto expected = line_matches(expression, text, limit);
      expect_same_lines(buffer_line_matches(expression, text, limit), expected);
      std::vector<std::size_t> one_byte;
      for (std::size_t i = 1; i <= text.size(); i++)
        one_byte.push_back(i);
      expect_same_lines(line_matches(expression, text, limit, one_byte),
                        expected);
    }
}

TEST(CgrepTest, BufferLineResetOwnershipAndLazyMachine) {
  std::string error = "untouched";
  auto bundle = cottontail::regexp::Cgrep::compile("cat", &error);
  auto storage = std::make_shared<const std::string>("first\ncat\ncat");
  std::weak_ptr<const std::string> owner = storage;
  std::shared_ptr<const char> buffer(storage, storage->data());
  const char *data = storage->data();
  auto matcher = cottontail::regexp::LineCgrep::make(
      bundle, buffer, storage->size(), 4, &error);
  ASSERT_NE(matcher, nullptr) << error;
  EXPECT_NE(bundle->buffer, nullptr);
  EXPECT_EQ(bundle->haystack, nullptr);
  storage.reset();
  buffer.reset();
  EXPECT_FALSE(owner.expired());
  cottontail::regexp::LineCgrep::Match match;
  for (int pass = 0; pass < 2; pass++) {
    ASSERT_TRUE(matcher->match(&match));
    EXPECT_EQ(match.p, 6);
    EXPECT_EQ(match.q, 8);
    EXPECT_EQ(match.start_line, 2u);
    EXPECT_EQ(match.end_line, 2u);
    const char *start;
    const char *end;
    ASSERT_TRUE(matcher->translate(match, &start, &end));
    EXPECT_EQ(start, data + 6);
    EXPECT_EQ(end, data + 10);
    ASSERT_TRUE(matcher->match(&match));
    EXPECT_EQ(match.start_line, 3u);
    EXPECT_EQ(matcher->translate(match), "cat");
    EXPECT_FALSE(matcher->match(&match));
    EXPECT_TRUE(matcher->success(&error));
    ASSERT_TRUE(matcher->reset(&error));
  }
  EXPECT_EQ(error, "untouched");
  matcher.reset();
  EXPECT_TRUE(owner.expired());
}

TEST(CgrepTest, BufferLinesRejectMissingTextAndRecover) {
  std::string error;
  auto bundle = cottontail::regexp::Cgrep::compile("a\\nb", &error);
  auto storage = std::make_shared<const std::string>("a\nb");
  std::shared_ptr<const char> buffer(storage, storage->data());
  auto matcher = cottontail::regexp::LineCgrep::make(
      bundle, buffer, storage->size(), 1, &error);
  ASSERT_NE(matcher, nullptr);
  cottontail::regexp::LineCgrep::Match match;
  ASSERT_TRUE(matcher->match(&match));
  EXPECT_FALSE(match.has_lines);
  EXPECT_EQ(match.p, 0);
  EXPECT_EQ(match.q, 2);
  EXPECT_EQ(matcher->translate(match), "");
  EXPECT_FALSE(matcher->success(&error));
  EXPECT_NE(error.find("no retained line text"), std::string::npos);
  EXPECT_FALSE(matcher->match(&match));
  ASSERT_TRUE(matcher->reset());
  EXPECT_TRUE(matcher->match(&match));
  ASSERT_TRUE(matcher->reset());
  EXPECT_FALSE(matcher->match(nullptr));
  EXPECT_FALSE(matcher->success());
  ASSERT_TRUE(matcher->reset());
  EXPECT_TRUE(matcher->match(&match));
}

TEST(CgrepTest, RejectsInvalidPublicMachine) {
  std::string error;
  cottontail::regexp::transition invalid{
      cottontail::regexp::final_state, cottontail::regexp::start_state, {'x'}};
  EXPECT_EQ(cottontail::regexp::Cgrep::compile({invalid}, &error), nullptr);
  EXPECT_NE(error.find("final state"), std::string::npos);
}
