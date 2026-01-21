#include <map>
#include <string>

#include <gtest/gtest.h>

#include "meadowlark/forager.h"
#include "src/cottontail.h"

static void ExpectRoundtrip(const std::string &name, const std::string &tag,
                            const std::map<std::string, std::string> &params) {
  const std::string j = cottontail::meadowlark::forager2json(name, tag, params);

  std::string n2, t2;
  std::map<std::string, std::string> p2;
  ASSERT_TRUE(cottontail::meadowlark::json2forager(j, &n2, &t2, &p2));
  EXPECT_EQ(n2, name);
  EXPECT_EQ(t2, tag);
  EXPECT_EQ(p2, params);
}

static void
ExpectParseOK(const std::string &json, const std::string &exp_name,
              const std::string &exp_tag,
              const std::map<std::string, std::string> &exp_params) {
  std::string n, t;
  std::map<std::string, std::string> p;
  ASSERT_TRUE(cottontail::meadowlark::json2forager(json, &n, &t, &p));
  EXPECT_EQ(n, exp_name);
  EXPECT_EQ(t, exp_tag);
  EXPECT_EQ(p, exp_params);
}

static void ExpectParseFail(const std::string &json) {
  std::string n, t;
  std::map<std::string, std::string> p;
  EXPECT_FALSE(cottontail::meadowlark::json2forager(json, &n, &t, &p));
}

TEST(ForagerJson, RoundtripEmptyParameters) {
  ExpectRoundtrip("alpha", "t0", {});
}

TEST(ForagerJson, RoundtripSimpleParameters) {
  ExpectRoundtrip("alpha", "t0", {{"k", "v"}, {"x", "y"}});
}

TEST(ForagerJson, RoundtripEscapesInNameTagKeysValues) {
  // Includes: quote, backslash, newline, tab, carriage return, control chars.
  ExpectRoundtrip("n\"ame\n", "t\\ag\t",
                  {
                      {"quote", "he said \"hi\""},
                      {"slash", "a\\b\\c"},
                      {"nl", "line1\nline2"},
                      {"tab", "a\tb"},
                      {"cr", "a\rb"},
                      {"bs", std::string("a\b", 2)},
                      {"ff", std::string("a\f", 2)},
                      {"key\"q", "val\"q"},
                      {"key\\b", "val\\b"},
                  });
}

TEST(ForagerJson, ParsesWithWeirdWhitespaceAndOrder) {
  const std::string j =
      "{\n"
      "  \"parameters\" : {  \"k\" : \"v\" , \"x\" : \"y\" },\n"
      "  \"tag\" : \"B\" ,\n"
      "  \"name\"  :  \"A\"\n"
      "}\n";
  ExpectParseOK(j, "A", "B", {{"k", "v"}, {"x", "y"}});
}

TEST(ForagerJson, MissingTagBecomesEmptyString) {
  const std::string j = "{\n"
                        "  \"name\": \"A\",\n"
                        "  \"parameters\": {\"k\":\"v\"}\n"
                        "}\n";
  ExpectParseOK(j, "A", "", {{"k", "v"}});
}

TEST(ForagerJson, MissingParametersBecomesEmptyMap) {
  const std::string j = "{\n"
                        "  \"name\": \"A\",\n"
                        "  \"tag\": \"B\"\n"
                        "}\n";
  ExpectParseOK(j, "A", "B", {});
}

TEST(ForagerJson, ExtraFieldsAreIgnored) {
  const std::string j = "{\n"
                        "  \"name\": \"A\",\n"
                        "  \"tag\": \"B\",\n"
                        "  \"extra\": 123,\n"
                        "  \"extra2\": [1,2,3],\n"
                        "  \"extra3\": {\"nested\": true},\n"
                        "  \"parameters\": {\"k\":\"v\"}\n"
                        "}\n";
  ExpectParseOK(j, "A", "B", {{"k", "v"}});
}

TEST(ForagerJson, DuplicateParameterKeysLastWins) {
  const std::string j =
      "{\n"
      "  \"name\": \"A\",\n"
      "  \"tag\": \"B\",\n"
      "  \"parameters\": {\"k\":\"v1\", \"k\":\"v2\", \"k\":\"v3\"}\n"
      "}\n";
  ExpectParseOK(j, "A", "B", {{"k", "v3"}});
}

TEST(ForagerJson, ParametersNonObjectIsIgnoredIfValidJson) {
  const std::string j = "{\n"
                        "  \"name\": \"A\",\n"
                        "  \"tag\": \"B\",\n"
                        "  \"parameters\": 7\n"
                        "}\n";
  ExpectParseOK(j, "A", "B", {});
}

TEST(ForagerJson, RejectsUnparseableTrash) {
  ExpectParseFail("\"name\":\"A\"");
  ExpectParseFail("{\"name\":\"A\"");                     // missing brace
  ExpectParseFail("{\"name\":\"A\",\"tag\":\"B\"");       // missing brace
  ExpectParseFail("{\"name\":\"A\",\"tag\":\"B\",}");     // trailing comma
  ExpectParseFail("{\"name\": \"A\", \"tag\": \"B\"} x"); // trailing garbage
  ExpectParseFail(
      "{\"name\":\"A\",\"tag\":\"B\",\"parameters\":{\"k\":\"v}}"); // unterminated
}

TEST(ForagerJson, RejectsMissingName) {
  const std::string j = "{\n"
                        "  \"tag\": \"B\",\n"
                        "  \"parameters\": {\"k\":\"v\"}\n"
                        "}\n";
  ExpectParseFail(j);
}

TEST(ForagerJson, SupportsUnicodeEscapesAsBytes) {
  // parse_string supports \uXXXX, and your writer emits \u00XX for control
  // chars.
  const std::string j = "{\n"
                        "  \"name\": \"A\\u0042\",\n" // "AB"
                        "  \"tag\": \"T\",\n"
                        "  \"parameters\": {\"k\":\"v\\u000a\"}\n" // "v\n"
                        "}\n";
  ExpectParseOK(j, "AB", "T", {{"k", "v\n"}});
}

TEST(ForagerJson, RoundtripControlCharsBecomeUnicodeEscapes) {
  // Ensure writer emits valid JSON and reader roundtrips.
  std::map<std::string, std::string> p;
  p["ctrl"] = std::string("\x01\x02\x1f", 3);
  ExpectRoundtrip("A", "B", p);
}
