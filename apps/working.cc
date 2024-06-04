#define ASSERT_EQ(a, b) (assert((a) == (b)))
#define ASSERT_NE(a, b) (assert((a) != (b)))
#define ASSERT_TRUE(a) (assert((a)))
#define ASSERT_FALSE(a) (assert(!(a)))
#define EXPECT_EQ(a, b) (assert((a) == (b)))
#define EXPECT_NE(a, b) (assert((a) != (b)))
#define EXPECT_GE(a, b) (assert((a) >= (b)))
#define EXPECT_GT(a, b) (assert((a) > (b)))
#define EXPECT_TRUE(a) (assert((a)))
#define EXPECT_FALSE(a) (assert(!(a)))

#include <cmath>
#define EXPECT_FLOAT_EQ(a, b) assert(fabs((a) - (b)) < 0.00000001)

#include <cstring>
#define EXPECT_STREQ(a, b) (assert(strcmp((a), (b)) == 0))
#define ASSERT_STREQ(a, b) (assert(strcmp((a), (b)) == 0))

#define TEST(a, b) void test_##a##_##b()

#include <algorithm>
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

#include "src/cottontail.h"
#include "src/json.h"

void usage(std::string program_name) {
  std::cerr << "usage: " << program_name << "\n";
}

TEST(JSON, Tokens) {
  std::string s =
      cottontail::open_object_token + cottontail::close_object_token +
      cottontail::open_array_token + cottontail::close_array_token +
      cottontail::open_string_token + cottontail::close_string_token +
      cottontail::colon_token + cottontail::comma_token +
      cottontail::open_number_token + cottontail::close_number_token;
  std::shared_ptr<cottontail::Tokenizer> tokenizer =
      cottontail::Tokenizer::make("utf8", "");
  ASSERT_NE(tokenizer, nullptr);
  std::vector<std::string> t = tokenizer->split(s);
  EXPECT_EQ(t.size(), 10);
  for (size_t i = 0; i < t.size(); i++)
    EXPECT_EQ(t[i].length(), 3);
}

std::string x0 =
    "{\n"
    "  \"id\": \"0001\",\n"
    "  \"type\": \"donut\",\n"
    "  \"name\": \"Cake\",\n"
    "  \"ppu\": 0.55,\n"
    "  \"batters\":\n"
    "    {\n"
    "      \"batter\":\n"
    "        [\n"
    "          { \"id\": \"1001\", \"type\": \"Regular\" },\n"
    "          { \"id\": \"1002\", \"type\": \"Chocolate\" },\n"
    "          { \"id\": \"1003\", \"type\": \"Blueberry\" },\n"
    "          { \"id\": \"1004\", \"type\": \"Devil's Food\" }\n"
    "        ]\n"
    "    },\n"
    "  \"topping\":\n"
    "    [\n"
    "      { \"id\": \"5001\", \"type\": \"None\" },\n"
    "      { \"id\": \"5002\", \"type\": \"Glazed\" },\n"
    "      { \"id\": \"5005\", \"type\": \"Sugar\" },\n"
    "      { \"id\": \"5007\", \"type\": \"Powdered Sugar\" },\n"
    "      { \"id\": \"5006\", \"type\": \"Chocolate with Sprinkles\" },\n"
    "      { \"id\": \"5003\", \"type\": \"Chocolate\" },\n"
    "      { \"id\": \"5004\", \"type\": \"Maple\" }\n"
    "    ]\n"
    "}";

std::string x1 = "{\n"
                 "  \"id\": \"0000\",\n"
                 "  \"type\": \"empty\",\n"
                 "  \"name\": \"\",\n"
                 "  \"void\": null,\n"
                 "  \"ppu\": -0.55,\n"
                 "  \"batters\":\n"
                 "    {\n"
                 "      \"batter\":\n"
                 "        [\n"
                 "        ]\n"
                 "    },\n"
                 "  \"topping\":\n"
                 "    [\n"
                 "      { },\n"
                 "      { },\n"
                 "      { }\n"
                 "    ]\n"
                 "}";

std::string x2 =
    "{\n"
    "  \"glossary\": {\n"
    "    \"title\": \"example glossary\",\n"
    "    \"GlossDiv\": {\n"
    "      \"title\": \"S\",\n"
    "      \"GlossList\": {\n"
    "        \"GlossEntry\": {\n"
    "          \"ID\": \"SGML\",\n"
    "          \"SortAs\": \"SGML\",\n"
    "          \"GlossTerm\": \"Standard Generalized Markup Language\",\n"
    "          \"Acronym\": \"SGML\",\n"
    "          \"Abbrev\": \"ISO 8879:1986\",\n"
    "          \"GlossDef\": {\n"
    "            \"para\": \"A meta-markup language\",\n"
    "            \"GlossSeeAlso\": [\"GML\", \"XML\"]\n"
    "           },\n"
    "           \"GlossSee\": \"markup\"\n"
    "        }\n"
    "      }\n"
    "    }\n"
    "  }\n"
    "}\n";

void scribe_warren(std::shared_ptr<cottontail::Warren> warren) {
  std::shared_ptr<cottontail::Scribe> scribe = cottontail::Scribe::make(warren);
  ASSERT_NE(scribe, nullptr);
  ASSERT_TRUE(scribe->transaction());
  json j0 = json::parse(x0);
  EXPECT_TRUE(json_scribe(j0, scribe));
  json j1 = json::parse(x1);
  EXPECT_TRUE(json_scribe(j1, scribe));
  json j2 = json::parse(x2);
  EXPECT_TRUE(json_scribe(j2, scribe));
  ASSERT_TRUE(scribe->ready());
  scribe->commit();
  scribe->finalize();
}

void check_warren(std::shared_ptr<cottontail::Warren> warren) {
  cottontail::addr p, q;
  cottontail::fval v;
  std::string t;
  warren->start();
  {
    std::shared_ptr<cottontail::Hopper> hopper =
        warren->hopper_from_gcl(":id:");
    ASSERT_NE(hopper, nullptr);
    hopper->tau(cottontail::minfinity + 1, &p, &q);
    t = cottontail::scribe_translate_json(warren->txt()->translate(p, q));
    EXPECT_EQ(t, "\"0001\"\n");
    hopper->tau(p + 1, &p, &q);
    t = cottontail::scribe_translate_json(warren->txt()->translate(p, q));
    EXPECT_EQ(t, "\"0000\"\n");
  }
  {
    std::shared_ptr<cottontail::Hopper> hopper = warren->hopper_from_gcl("id:");
    ASSERT_NE(hopper, nullptr);
    size_t i = 0;
    for (hopper->tau(cottontail::minfinity + 1, &p, &q);
         p < cottontail::maxfinity; hopper->tau(p + 1, &p, &q)) {
      t = cottontail::scribe_translate_json(warren->txt()->translate(p, q));
      EXPECT_EQ(t.length(), 7);
      i++;
    }
    EXPECT_EQ(i, 13);
    EXPECT_EQ(t, "\"0000\"\n");
  }
  {
    std::shared_ptr<cottontail::Hopper> hopper =
        warren->hopper_from_gcl(":ppu:");
    ASSERT_NE(hopper, nullptr);
    hopper->tau(cottontail::minfinity + 1, &p, &q, &v);
    EXPECT_EQ(v, 0.55);
    t = cottontail::scribe_translate_json(warren->txt()->translate(p, q));
    EXPECT_EQ(t, "0.550000\n");
    hopper->tau(p + 1, &p, &q, &v);
    EXPECT_EQ(v, -0.55);
    t = cottontail::scribe_translate_json(warren->txt()->translate(p, q));
    EXPECT_EQ(t, "-0.550000\n");
  }
  {
    std::shared_ptr<cottontail::Hopper> hopper =
        warren->hopper_from_gcl(":batters:batter:");
    ASSERT_NE(hopper, nullptr);
    hopper->tau(cottontail::minfinity + 1, &p, &q, &v);
    EXPECT_EQ(v, 4);
    hopper->tau(p + 1, &p, &q, &v);
    EXPECT_EQ(v, 0);
    t = cottontail::scribe_translate_json(warren->txt()->translate(p, q));
    EXPECT_EQ(t, "[\n]\n");
  }
  {
    std::shared_ptr<cottontail::Hopper> hopper =
        warren->hopper_from_gcl("(<< type: (+ :batters:batter: :topping:))");
    ASSERT_NE(hopper, nullptr);
    size_t i = 0;
    for (hopper->tau(cottontail::minfinity + 1, &p, &q);
         p < cottontail::maxfinity; hopper->tau(p + 1, &p, &q)) {
      t = cottontail::scribe_translate_json(warren->txt()->translate(p, q));
      EXPECT_GT(t.length(), 6);
      i++;
    }
    EXPECT_EQ(i, 11);
  }
  {
    std::shared_ptr<cottontail::Hopper> hopper =
        warren->hopper_from_gcl(":void:");
    ASSERT_NE(hopper, nullptr);
    hopper->tau(cottontail::minfinity + 1, &p, &q, &v);
    EXPECT_TRUE(std::isnan(v));
    t = cottontail::scribe_translate_json(warren->txt()->translate(p, q));
    EXPECT_EQ(t, "null\n");
  }
  {
    std::shared_ptr<cottontail::Hopper> hopper =
        warren->hopper_from_gcl("\"SGML\"");
    ASSERT_NE(hopper, nullptr);
    size_t i = 0;
    for (hopper->tau(cottontail::minfinity + 1, &p, &q);
         p < cottontail::maxfinity; hopper->tau(p + 1, &p, &q)) {
      t = cottontail::scribe_translate_json(warren->txt()->translate(p, q));
      EXPECT_EQ(t, "SGML");
      i++;
    }
    EXPECT_EQ(i, 3);
  }
  warren->end();
}

TEST(JSON, Bigwig) {
  std::string error;
  std::string burrow = "bigwig.json.burrow";
  std::shared_ptr<cottontail::Bigwig> bigwig =
      cottontail::Bigwig::make(burrow, "txt:json:yes", &error);
  ASSERT_NE(bigwig, nullptr);
  scribe_warren(bigwig);
  check_warren(bigwig);
}

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  std::string error;
  test_JSON_Tokens();
  test_JSON_Bigwig();
  return 0;
}
