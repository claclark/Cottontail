#include <set>
#include <string>
#include <utility>
#include <vector>

#include "gtest/gtest.h"

#include "regexp/nfa.h"

namespace {

using cottontail::regexp::final_state;
using cottontail::regexp::match;
using cottontail::regexp::nfa;
using cottontail::regexp::start_state;
using cottontail::regexp::transition;

std::vector<std::pair<std::size_t, std::size_t>> matches(
    const std::string &expression, const std::string &text) {
  std::string error;
  std::vector<transition> machine = nfa(expression, &error);
  EXPECT_FALSE(machine.empty()) << error;
  return match(machine, text);
}

} // namespace

TEST(NfaTest, BuildsLiteralMachine) {
  std::string error;
  std::vector<transition> machine = nfa("ab", &error);
  ASSERT_EQ(error, "");
  ASSERT_EQ(machine.size(), std::size_t{2});
  EXPECT_EQ(machine[0].from, cottontail::regexp::state{1});
  EXPECT_EQ(machine[0].to, final_state);
  EXPECT_EQ(machine[0].characters, std::set<unsigned char>({'b'}));
  EXPECT_EQ(machine[1].from, start_state);
  EXPECT_EQ(machine[1].to, cottontail::regexp::state{1});
  EXPECT_EQ(machine[1].characters, std::set<unsigned char>({'a'}));
}

TEST(NfaTest, CombinesAlternativeLabels) {
  std::string error;
  std::vector<transition> machine = nfa("a|b", &error);
  ASSERT_EQ(error, "");
  ASSERT_EQ(machine.size(), std::size_t{1});
  EXPECT_EQ(machine[0].from, start_state);
  EXPECT_EQ(machine[0].to, final_state);
  EXPECT_FALSE(machine[0].complement);
  EXPECT_EQ(machine[0].characters,
            std::set<unsigned char>({'a', 'b'}));
}

TEST(NfaTest, RepresentsDotAndComplementedClass) {
  std::string error;
  std::vector<transition> dot = nfa(".", &error);
  ASSERT_EQ(error, "");
  ASSERT_EQ(dot.size(), std::size_t{1});
  EXPECT_TRUE(dot[0].complement);
  EXPECT_TRUE(dot[0].characters.empty());

  std::vector<transition> not_digits = nfa("[^0-9]", &error);
  ASSERT_EQ(error, "");
  ASSERT_EQ(not_digits.size(), std::size_t{1});
  EXPECT_TRUE(not_digits[0].complement);
  EXPECT_EQ(not_digits[0].characters.size(), std::size_t{10});
}

TEST(NfaTest, RejectsLambdaAndInvalidExpressions) {
  std::string error;
  EXPECT_TRUE(nfa("a*", &error).empty());
  EXPECT_NE(error.find("lambda"), std::string::npos);

  EXPECT_TRUE(nfa("[z-a]", &error).empty());
  EXPECT_NE(error.find("range"), std::string::npos);

  EXPECT_TRUE(nfa("a\\", &error).empty());
  EXPECT_NE(error.find("backslash"), std::string::npos);

  EXPECT_TRUE(nfa("a&b", &error).empty());
  EXPECT_NE(error.find("no strings"), std::string::npos);
}

TEST(NfaTest, MatchesShortestSubstrings) {
  EXPECT_EQ(matches("ab|b", "ab"),
            (std::vector<std::pair<std::size_t, std::size_t>>{{1, 1}}));
  EXPECT_EQ(matches("ab|abc", "abc"),
            (std::vector<std::pair<std::size_t, std::size_t>>{{0, 1}}));
  EXPECT_EQ(matches("a+", "aaa"),
            (std::vector<std::pair<std::size_t, std::size_t>>{
                {0, 0}, {1, 1}, {2, 2}}));
}

TEST(NfaTest, ReturnsOverlappingInclusiveIntervals) {
  EXPECT_EQ(matches(" cat ", " cat cat cat "),
            (std::vector<std::pair<std::size_t, std::size_t>>{
                {0, 4}, {4, 8}, {8, 12}}));
  EXPECT_EQ(matches("aba", "ababa"),
            (std::vector<std::pair<std::size_t, std::size_t>>{
                {0, 2}, {2, 4}}));
}

TEST(NfaTest, SupportsClassesEscapesAndIntersection) {
  EXPECT_EQ(matches("[a-c]\\d", "a1 d2 c3"),
            (std::vector<std::pair<std::size_t, std::size_t>>{
                {0, 1}, {6, 7}}));
  EXPECT_EQ(matches("a.+c", "a\nc axc"),
            (std::vector<std::pair<std::size_t, std::size_t>>{{0, 2},
                                                              {4, 6}}));
  EXPECT_EQ(matches("a+&aa", "aaa"),
            (std::vector<std::pair<std::size_t, std::size_t>>{{0, 1},
                                                              {1, 2}}));
}

TEST(NfaTest, IntersectsContainmentWithMultilineText) {
  std::string text =
      "preface\nThe the first\nordinary\nthe The second\nepilogue\n";
  EXPECT_EQ(matches("\\n.*\\n&.*[Tt]he [Tt]he.*", text),
            (std::vector<std::pair<std::size_t, std::size_t>>{{7, 21},
                                                              {30, 45}}));
}

TEST(NfaTest, ComposesNullableSubexpressions) {
  EXPECT_EQ(matches("a?b", "ab b"),
            (std::vector<std::pair<std::size_t, std::size_t>>{{1, 1},
                                                              {3, 3}}));
  EXPECT_EQ(matches("(a|b)+c", "abbc ac"),
            (std::vector<std::pair<std::size_t, std::size_t>>{{2, 3},
                                                              {5, 6}}));
}

TEST(NfaTest, MatchesArbitraryBytes) {
  std::string text("a\0b", 3);
  EXPECT_EQ(matches("\\x00", text),
            (std::vector<std::pair<std::size_t, std::size_t>>{{1, 1}}));
}

TEST(NfaTest, MatchesFrenchUtf8Literals) {
  EXPECT_EQ(matches("café", "café café"),
            (std::vector<std::pair<std::size_t, std::size_t>>{{0, 4},
                                                              {6, 10}}));
  EXPECT_EQ(matches("café|fé", "café café"),
            (std::vector<std::pair<std::size_t, std::size_t>>{{2, 4},
                                                              {8, 10}}));
}

TEST(NfaTest, MatchesChineseUtf8Literals) {
  EXPECT_EQ(matches("中国中", "中国中国中"),
            (std::vector<std::pair<std::size_t, std::size_t>>{{0, 8},
                                                              {6, 14}}));
  EXPECT_EQ(matches("中国|国", "中国"),
            (std::vector<std::pair<std::size_t, std::size_t>>{{3, 5}}));
}

TEST(NfaTest, MatchesEmojiUtf8Literals) {
  EXPECT_EQ(matches("❤️", "I ❤️ NY"),
            (std::vector<std::pair<std::size_t, std::size_t>>{{2, 7}}));
  EXPECT_EQ(matches("🤖", "two 🤖s 🤖"),
            (std::vector<std::pair<std::size_t, std::size_t>>{{4, 7},
                                                              {10, 13}}));
}
