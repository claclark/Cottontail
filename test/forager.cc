#include <map>
#include <string>

#include <gtest/gtest.h>

#include "meadowlark/metadata.h"
#include "src/cottontail.h"

static void ExpectRoundtrip(const std::string &name, const std::string &tag,
                            const std::map<std::string, std::string> &params) {
  const std::string query = ":contents:";
  const std::string j =
      cottontail::meadowlark::forager2json(name, tag, query, params);

  cottontail::meadowlark::ForagerMetadata metadata;
  ASSERT_TRUE(cottontail::meadowlark::json2forager(j, &metadata));
  EXPECT_EQ(metadata.name, name);
  EXPECT_EQ(metadata.tag, tag);
  EXPECT_TRUE(metadata.has_query);
  EXPECT_EQ(metadata.query, query);
  EXPECT_FALSE(metadata.has_filename);
  EXPECT_EQ(metadata.parameters, params);
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

TEST(JsonMetadata, DescribesNormalizedFile) {
  const std::string j = cottontail::meadowlark::json_metadata("./whatever");
  EXPECT_NE(j.find("\"type\": \"json\""), std::string::npos);
  EXPECT_NE(j.find("\"filename\": \"./whatever\""), std::string::npos);
}

TEST(TextMetadata, DescribesNormalizedFile) {
  const std::string j = cottontail::meadowlark::text_metadata("./whatever");
  EXPECT_NE(j.find("\"type\": \"text\""), std::string::npos);
  EXPECT_NE(j.find("\"filename\": \"./whatever\""), std::string::npos);
}

TEST(CodeMetadata, DescribesNormalizedFile) {
  const std::string j = cottontail::meadowlark::code_metadata("./whatever");
  EXPECT_NE(j.find("\"type\": \"code\""), std::string::npos);
  EXPECT_NE(j.find("\"filename\": \"./whatever\""), std::string::npos);
}

TEST(FileMetadata, ReadsCurrentAndHistoricalFilenameFields) {
  std::string type, filename;
  ASSERT_TRUE(cottontail::meadowlark::json2file(
      "{\"type\":\"text\",\"filename\":\"./new.txt\"}", &type,
      &filename));
  EXPECT_EQ(type, "text");
  EXPECT_EQ(filename, "./new.txt");
  ASSERT_TRUE(cottontail::meadowlark::json2file(
      "{\"type\":\"text\",\"file\":\"./old.txt\"}", &type,
      &filename));
  EXPECT_EQ(type, "text");
  EXPECT_EQ(filename, "./old.txt");
}

TEST(TsvMetadata, DescribesHeaderMapping) {
  const std::string j = cottontail::meadowlark::tsv_metadata(
      "./table.tsv", "\t", true, {"Animal", "Favorite Food"},
      {":Animal:", ":Favorite_Food:"});
  EXPECT_NE(j.find("\"type\": \"tsv\""), std::string::npos);
  EXPECT_NE(j.find("\"filename\": \"./table.tsv\""), std::string::npos);
  EXPECT_NE(j.find("\"separator\": \"\\t\""), std::string::npos);
  EXPECT_NE(j.find("\"header\": true"), std::string::npos);
  EXPECT_NE(j.find("\"header\": \"Favorite Food\""), std::string::npos);
  EXPECT_NE(j.find("\"feature\": \":Favorite_Food:\""),
            std::string::npos);
}

TEST(TsvMetadata, OmitsHeadingsWithoutHeader) {
  const std::string j = cottontail::meadowlark::tsv_metadata(
      "./table.tsv", "\t", false, {"", ""}, {":0:", ":1:"});
  EXPECT_NE(j.find("\"header\": false"), std::string::npos);
  EXPECT_EQ(j.find("\"header\": \""), std::string::npos);
  EXPECT_NE(j.find("\"feature\": \":0:\""), std::string::npos);
  EXPECT_NE(j.find("\"feature\": \":1:\""), std::string::npos);
}

TEST(ForagerJson, WriterIncludesForagerType) {
  const std::string j =
      cottontail::meadowlark::forager2json("alpha", "t0", ":", {});
  EXPECT_NE(j.find("\"type\": \"forager\""), std::string::npos);
}

TEST(ForagerJson, FileRecordOmitsDefinitionFields) {
  const std::string j = cottontail::meadowlark::forager_file2json(
      "./whatever", "alpha", "t0");
  cottontail::meadowlark::ForagerMetadata metadata;
  ASSERT_TRUE(cottontail::meadowlark::json2forager(j, &metadata));
  EXPECT_EQ(metadata.filename, "./whatever");
  EXPECT_TRUE(metadata.has_filename);
  EXPECT_FALSE(metadata.has_query);
  EXPECT_TRUE(metadata.parameters.empty());
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

TEST(ForagerJson, ExplicitForagerTypeIsAccepted) {
  const std::string j = "{\n"
                        "  \"type\": \"forager\",\n"
                        "  \"name\": \"A\",\n"
                        "  \"tag\": \"B\"\n"
                        "}\n";
  ExpectParseOK(j, "A", "B", {});
}

TEST(ForagerJson, OtherMetadataTypesAreRejected) {
  ExpectParseFail("{\"type\":\"tsv\",\"name\":\"A\"}");
  ExpectParseFail("{\"type\":7,\"name\":\"A\"}");
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
