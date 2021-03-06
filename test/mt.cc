#include "gtest/gtest.h"

#include "src/mt.h"

TEST(MtTest, Parsing) {
  cottontail::Mt mt;
  ASSERT_TRUE(mt.infix_expression(
      "(\"foo\" and \"bar\") or (\"hello\" ...  \"world\")"));
  ASSERT_EQ(mt.s_expression(),
            "(+ (^ \"foo\" \"bar\") (... \"hello\" \"world\"))");
  ASSERT_TRUE(mt.infix_expression("\"foo\" and \"bar\""));
  ASSERT_EQ(mt.s_expression(), "(^ \"foo\" \"bar\")");
  ASSERT_TRUE(mt.infix_expression("\"foo\" or \"bar\""));
  ASSERT_EQ(mt.s_expression(), "(+ \"foo\" \"bar\")");
  ASSERT_TRUE(mt.infix_expression("\"foo\" <> \"bar\""));
  ASSERT_EQ(mt.s_expression(), "(... \"foo\" \"bar\")");
  ASSERT_TRUE(mt.infix_expression("foo = \"foo\""));
  ASSERT_EQ(mt.s_expression(), "");
  ASSERT_TRUE(mt.infix_expression("foo"));
  ASSERT_EQ(mt.s_expression(), "\"foo\"");
  ASSERT_TRUE(mt.infix_expression("bar = \"bar\""));
  ASSERT_EQ(mt.s_expression(), "");
  ASSERT_TRUE(mt.infix_expression("bar"));
  ASSERT_EQ(mt.s_expression(), "\"bar\"");
  ASSERT_TRUE(mt.infix_expression("bar contained in foo"));
  ASSERT_EQ(mt.s_expression(), "(<< \"bar\" \"foo\")");
  ASSERT_TRUE(mt.infix_expression("bar containing foo"));
  ASSERT_EQ(mt.s_expression(), "(>> \"bar\" \"foo\")");
  ASSERT_TRUE(mt.infix_expression("bar not contained in foo"));
  ASSERT_EQ(mt.s_expression(), "(!< \"bar\" \"foo\")");
  ASSERT_TRUE(mt.infix_expression("bar not containing foo"));
  ASSERT_EQ(mt.s_expression(), "(!> \"bar\" \"foo\")");
  ASSERT_TRUE(mt.infix_expression("2 of (foo, \"hello\", bar)"));
  ASSERT_EQ(mt.s_expression(), "(2 \"bar\" \"hello\" \"foo\")");
  ASSERT_TRUE(mt.infix_expression("1 of (foo, \"hello\", bar)"));
  ASSERT_EQ(mt.s_expression(), "(+ \"bar\" \"hello\" \"foo\")");
  ASSERT_TRUE(mt.infix_expression("all of (foo, \"hello\", bar)"));
  ASSERT_EQ(mt.s_expression(), "(^ \"bar\" \"hello\" \"foo\")");
  ASSERT_TRUE(mt.infix_expression("3 of (foo, \"hello\", bar)"));
  ASSERT_EQ(mt.s_expression(), "(^ \"bar\" \"hello\" \"foo\")");
  ASSERT_TRUE(mt.infix_expression("[100]"));
  ASSERT_FALSE(mt.infix_expression("sososososososo *&(&(&*(&"));
  std::string error;
  ASSERT_FALSE(mt.infix_expression("sososososososo *&(&(&*(&", &error));
  ASSERT_EQ(error, "Undefined symbol.");
  ASSERT_FALSE(mt.infix_expression("2 of foo, \"hello\", bar)", &error));
  ASSERT_EQ(error, "Missing \"(\" after start of combination operator.");
  ASSERT_FALSE(mt.infix_expression("2 of (foo, \"hello\", bar", &error));
  ASSERT_EQ(error, "Missing \")\".");
  ASSERT_FALSE(mt.infix_expression("999 of (foo, \"hello\", bar)", &error));
  ASSERT_EQ(error,
            "Too few elements on right-hand side of combination operator.");
  ASSERT_FALSE(mt.infix_expression("one words", &error));
  ASSERT_EQ(error, "Expected \"^\" or \"of\" after \"one\".");
  ASSERT_FALSE(mt.infix_expression("all words", &error));
  ASSERT_EQ(error, "Expected \"^\" or \"of\" after \"all\".");
  ASSERT_FALSE(mt.infix_expression("0 words", &error));
  ASSERT_EQ(error, "The expression \"0 words\" is not valid.");
  ASSERT_FALSE(mt.infix_expression("99 bottles of beer", &error));
  ASSERT_EQ(error, "Integer not followed by  \"^\" or \"of\" or \"words\".");
  ASSERT_FALSE(mt.infix_expression("x and y", &error));
  ASSERT_EQ(error, "Undefined symbol.");
  ASSERT_FALSE(mt.infix_expression("(\"foo\" and \"bar\"", &error));
  ASSERT_EQ(error, "Missing \")\".");
  ASSERT_FALSE(mt.infix_expression("foo and (bar or [0])", &error));
  ASSERT_EQ(error, "The expression \"[0]\" is not valid.");
  ASSERT_TRUE(mt.infix_expression("99 words", &error));
  ASSERT_EQ(mt.s_expression(), "(# 99)");
  ASSERT_TRUE(mt.infix_expression("foo and (bar or [10])", &error));
  ASSERT_EQ(mt.s_expression(), "(^ \"foo\" (+ \"bar\" (# 10)))");
  ASSERT_FALSE(mt.infix_expression("foo and (bar or [10)", &error));
  ASSERT_EQ(error, "Missing \"]\".");
  ASSERT_FALSE(mt.infix_expression("foo and (bar or [iiiii])", &error));
  ASSERT_EQ(error, "Missing integer after \"[\".");
  ASSERT_FALSE(mt.infix_expression("2 of (foo, \"hello, bar)", &error));
  ASSERT_EQ(error, "End of quoted string missing.");
  ASSERT_FALSE(mt.infix_expression("2 of (foo, \"hello\", bar)  ....", &error));
  ASSERT_EQ(error, "Unexpected character in input.");
}
