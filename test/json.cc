#include <memory>
#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "src/cottontail.h"
#include "src/json.h"

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
  const char *c = s.c_str();
  for (size_t i = 0; i < 10; i++)
    EXPECT_TRUE(cottontail::json_internal_token(c + 3 * i, 3));
  EXPECT_FALSE(cottontail::json_internal_token(nullptr, 2));
  EXPECT_FALSE(cottontail::json_internal_token("the", 3));
}

TEST(JSON, Encode) {
  EXPECT_EQ(cottontail::json_encode(""), cottontail::open_string_token + "" +
                                             cottontail::close_string_token);
  EXPECT_EQ(cottontail::json_encode("simple"),
            cottontail::open_string_token + "simple" +
                cottontail::close_string_token);
  EXPECT_EQ(cottontail::json_encode("hello world"),
            cottontail::open_string_token + "hello world" +
                cottontail::close_string_token);
  EXPECT_EQ(cottontail::json_encode("\""), cottontail::open_string_token +
                                               "\\\"" +
                                               cottontail::close_string_token);
  EXPECT_EQ(cottontail::json_encode("\\"), cottontail::open_string_token +
                                               "\\\\" +
                                               cottontail::close_string_token);
  EXPECT_EQ(cottontail::json_encode("/"), cottontail::open_string_token + "/" +
                                              cottontail::close_string_token);
  EXPECT_EQ(cottontail::json_encode("path/to/resource"),
            cottontail::open_string_token + "path/to/resource" +
                cottontail::close_string_token);
  EXPECT_EQ(cottontail::json_encode("line\nbreak"),
            cottontail::open_string_token + "line\\nbreak" +
                cottontail::close_string_token);
  EXPECT_EQ(cottontail::json_encode("\b\f\n\r\t"),
            cottontail::open_string_token + "\\b\\f\\n\\r\\t" +
                cottontail::close_string_token);
  EXPECT_EQ(cottontail::json_encode("\x1F"),
            cottontail::open_string_token + "\\u001f" +
                cottontail::close_string_token);
  EXPECT_EQ(cottontail::json_encode("\x01"),
            cottontail::open_string_token + "\\u0001" +
                cottontail::close_string_token);
  EXPECT_EQ(cottontail::json_encode("emoji: ☺"),
            cottontail::open_string_token + "emoji: ☺" +
                cottontail::close_string_token);
  EXPECT_EQ(cottontail::json_encode("mix\"/\\\b"),
            cottontail::open_string_token + "mix\\\"/\\\\\\b" +
                cottontail::close_string_token);
}

namespace {

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

void scribe_json(std::shared_ptr<cottontail::Scribe> scribe) {
  ASSERT_TRUE(scribe->transaction());
  EXPECT_TRUE(json_scribe(x0, scribe));
  EXPECT_TRUE(json_scribe(x1, scribe));
  EXPECT_TRUE(json_scribe(x2, scribe));
  ASSERT_TRUE(scribe->ready());
  scribe->commit();
  scribe->finalize();
}

void check_json(std::shared_ptr<cottontail::Warren> warren) {
  cottontail::addr p, q;
  cottontail::fval v;
  std::string t;
  warren->start();
  {
    std::shared_ptr<cottontail::Hopper> hopper =
        warren->hopper_from_gcl(":id:");
    ASSERT_NE(hopper, nullptr);
    hopper->tau(cottontail::minfinity + 1, &p, &q);
    t = cottontail::json_translate(warren->txt()->translate(p, q));
    EXPECT_EQ(t, "\"0001\"");
    hopper->tau(p + 1, &p, &q);
    t = cottontail::json_translate(warren->txt()->translate(p, q));
    EXPECT_EQ(t, "\"0000\"");
  }
  {
    std::shared_ptr<cottontail::Hopper> hopper = warren->hopper_from_gcl("id:");
    ASSERT_NE(hopper, nullptr);
    size_t i = 0;
    for (hopper->tau(cottontail::minfinity + 1, &p, &q);
         p < cottontail::maxfinity; hopper->tau(p + 1, &p, &q)) {
      t = cottontail::json_translate(warren->txt()->translate(p, q));
      EXPECT_EQ(t.length(), 6);
      i++;
    }
    EXPECT_EQ(i, 13);
    EXPECT_EQ(t, "\"0000\"");
  }
  {
    std::shared_ptr<cottontail::Hopper> hopper =
        warren->hopper_from_gcl(":ppu:");
    ASSERT_NE(hopper, nullptr);
    hopper->tau(cottontail::minfinity + 1, &p, &q, &v);
    EXPECT_EQ(v, 0.55);
    t = cottontail::json_translate(warren->txt()->translate(p, q));
    EXPECT_EQ(t, "0.550000");
    hopper->tau(p + 1, &p, &q, &v);
    EXPECT_EQ(v, -0.55);
    t = cottontail::json_translate(warren->txt()->translate(p, q));
    EXPECT_EQ(t, "-0.550000");
  }
  {
    std::shared_ptr<cottontail::Hopper> hopper =
        warren->hopper_from_gcl(":batters:batter:");
    ASSERT_NE(hopper, nullptr);
    hopper->tau(cottontail::minfinity + 1, &p, &q, &v);
    EXPECT_EQ(v, 4);
    hopper->tau(p + 1, &p, &q, &v);
    EXPECT_EQ(v, 0);
    t = cottontail::json_translate(warren->txt()->translate(p, q));
    EXPECT_EQ(t, "[ ]");
  }
  {
    std::shared_ptr<cottontail::Hopper> hopper =
        warren->hopper_from_gcl("(<< type: (+ :batters:batter: :topping:))");
    ASSERT_NE(hopper, nullptr);
    size_t i = 0;
    for (hopper->tau(cottontail::minfinity + 1, &p, &q);
         p < cottontail::maxfinity; hopper->tau(p + 1, &p, &q)) {
      t = cottontail::json_translate(warren->txt()->translate(p, q));
      EXPECT_GT(t.length(), 5);
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
    t = cottontail::json_translate(warren->txt()->translate(p, q));
    EXPECT_EQ(t, "null");
  }
  {
    std::shared_ptr<cottontail::Hopper> hopper =
        warren->hopper_from_gcl("\"SGML\"");
    ASSERT_NE(hopper, nullptr);
    size_t i = 0;
    for (hopper->tau(cottontail::minfinity + 1, &p, &q);
         p < cottontail::maxfinity; hopper->tau(p + 1, &p, &q)) {
      t = cottontail::json_translate(warren->txt()->translate(p, q));
      EXPECT_EQ(t, "SGML");
      i++;
    }
    EXPECT_EQ(i, 3);
  }
  warren->end();
}

void check_books(std::shared_ptr<cottontail::Warren> warren) {
  warren->start();
  std::unique_ptr<cottontail::Hopper> books =
      warren->hopper_from_gcl("(<< : test/books.json)");
  std::unique_ptr<cottontail::Hopper> title =
      warren->hopper_from_gcl(":title:");
  std::vector<std::unique_ptr<cottontail::Hopper>> authors;
  std::vector<std::pair<std::string, std::string>> results;
  cottontail::addr p, q;
  for (books->tau(cottontail::minfinity + 1, &p, &q); q < cottontail::maxfinity;
       books->tau(p + 1, &p, &q)) {
    cottontail::addr p_title, q_title;
    title->tau(p, &p_title, &q_title);
    if (q_title > q)
      continue;
    std::string the_title = warren->txt()->translate(p_title, q_title);
    if (the_title == "\"\"")
      continue;
    for (size_t i = 0;; i++) {
      if (authors.size() <= i) {
        std::string feature = ":authors:[" + std::to_string(i) + "]:";
        authors.emplace_back(warren->hopper_from_gcl(feature));
      }
      cottontail::addr p_author, q_author;
      authors[i]->tau(p, &p_author, &q_author);
      if (q_author > q)
        break;
      std::string the_author = warren->txt()->translate(p_author, q_author);
      if (the_author == "\"\"")
        continue;
      results.emplace_back(the_title, the_author);
    }
  }
  warren->end();
  EXPECT_EQ(results[688].first, "\"jQuery UI in Action\"");
  EXPECT_EQ(results[688].second, "\"Theodore J. (T.J.) VanToll III\"");
  EXPECT_EQ(results[710].first, "\"Barcodes with iOS\"");
  EXPECT_EQ(results[710].second, "\"Oliver Drobnik\"");
}
} // namespace

TEST(JSON, Bigwig) {
  std::string error;
  std::string burrow = "bigwig.json.burrow";
  std::shared_ptr<cottontail::Bigwig> bigwig =
      cottontail::Bigwig::make(burrow, "txt:json:yes", &error);
  ASSERT_NE(bigwig, nullptr);
  std::shared_ptr<cottontail::Scribe> scribe = cottontail::Scribe::make(bigwig);
  ASSERT_NE(scribe, nullptr);
  scribe_json(scribe);
  check_json(bigwig);
  std::vector<std::string> filenames;
  filenames.push_back("test/books.json");
  ASSERT_TRUE(scribe_jsonl(filenames, scribe));
  check_books(bigwig);
}

TEST(JSON, Simple) {
  std::string error;
  std::string burrow = "simple.json.burrow";
  std::shared_ptr<cottontail::Working> working =
      cottontail::Working::mkdir(burrow, &error);
  ASSERT_NE(working, nullptr);
  std::string options = "tokenizer:name:utf8 txt:json:yes";
  std::shared_ptr<cottontail::Builder> builder =
      cottontail::SimpleBuilder::make(working, options, &error);
  ASSERT_NE(builder, nullptr);
  std::shared_ptr<cottontail::Scribe> scribe =
      cottontail::Scribe::make(builder, &error);
  ASSERT_NE(scribe, nullptr);
  scribe_json(scribe);
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::Warren::make(burrow, &error);
  ASSERT_NE(warren, nullptr);
  check_json(warren);
}
