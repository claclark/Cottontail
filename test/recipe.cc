#include <map>
#include <string>

#include "gtest/gtest.h"

#include "src/recipe.h"

TEST(recipe, Basic) {
  std::string test0 = "["
                      "  hello:'abc',"
                      "  there:'xyz',"
                      "  world: ["
                      "    first:'1',"
                      "    second:'2',"
                      "  ]"
                      "]";
  size_t failure;
  std::map<std::string, std::string> a;
  EXPECT_TRUE(cottontail::cook(test0, &a));
  EXPECT_EQ(a["hello"], "abc");
  EXPECT_EQ(a["there"], "xyz");
  std::map<std::string, std::string> b;
  EXPECT_TRUE(cottontail::cook(a["world"], &b));
  EXPECT_EQ(b["first"], "1");
  EXPECT_EQ(b["second"], "2");
  for (size_t i = 0; i < test0.size() - 1; i++) {
    std::string bad = test0.substr(0, i);
    EXPECT_FALSE(cottontail::cook(bad, &a, &failure));
    EXPECT_EQ(failure, i);
    EXPECT_EQ(a["hello"], "abc");
    EXPECT_EQ(a["there"], "xyz");
  }
  std::string test1 = "["
                      "  hello          :               'a\\\\\\'bc',"
                      "  there   :\"xyz\"                 ,"
                      "  world: ["
                      "    first :'1' ,"
                      " "
                      "    second: '2',"
                      "  ]     "
                      "   ]";
  std::map<std::string, std::string> c;
  EXPECT_TRUE(cottontail::cook(test1, &c));
  EXPECT_EQ(c["hello"], "a\\\'bc");
  EXPECT_EQ(c["there"], "xyz");
  std::map<std::string, std::string> d;
  EXPECT_TRUE(cottontail::cook(c["world"], &d));
  EXPECT_EQ(d["first"], "1");
  EXPECT_EQ(d["second"], "2");
  for (size_t i = 0; i < test0.size() - 1; i++) {
    std::string bad = test1.substr(0, i);
    EXPECT_FALSE(cottontail::cook(bad, &c, &failure));
    EXPECT_EQ(failure, i);
    EXPECT_EQ(c["hello"], "a\\'bc");
    EXPECT_EQ(c["there"], "xyz");
  }
  std::string a_frozen = cottontail::freeze(a);
  std::map<std::string, std::string> e;
  EXPECT_TRUE(cottontail::cook(a_frozen, &e));
  EXPECT_EQ(e["hello"], "abc");
  EXPECT_EQ(e["there"], "xyz");
  std::map<std::string, std::string> f;
  EXPECT_TRUE(cottontail::cook(e["world"], &f));
  EXPECT_EQ(f["first"], "1");
  EXPECT_EQ(f["second"], "2");
  std::string test2 = "["
                      "  featurizer:["
                      "    name:\"hashing\","
                      "    recipe:\"\","
                      "  ],"
                      "  idx:["
                      "    name:\"simple\","
                      "    recipe:["
                      "      fvalue_compressor:\"zlib\","
                      "      fvalue_compressor_recipe:\"\","
                      "      posting_compressor:\"post\","
                      "      posting_compressor_recipe:\"\","
                      "    ],"
                      "  ],"
                      "  tokenizer:["
                      "    name:\"ascii\","
                      "    recipe:\"xml\","
                      "  ],"
                      "  txt:["
                      "    name:\"simple\","
                      "    recipe:["
                      "      compressor:\"zlib\","
                      "      compressor_recipe:\"\","
                      "    ],"
                      "  ],"
                      "  warren:\"simple\","
                      "]";
  std::map<std::string, std::string> g;
  EXPECT_TRUE(cottontail::cook(test2, &g));
  EXPECT_EQ(g["warren"], "simple");
  std::map<std::string, std::string> h;
  EXPECT_TRUE(cottontail::cook(g["idx"], &h));
  EXPECT_EQ(h["name"], "simple");
  std::map<std::string, std::string> i;
  EXPECT_TRUE(cottontail::cook(h["recipe"], &i));
  EXPECT_EQ(i["fvalue_compressor"], "zlib");
  EXPECT_EQ(i["posting_compressor_recipe"], "");
  std::string g_frozen = cottontail::freeze(g);
  std::map<std::string, std::string> j;
  EXPECT_TRUE(cottontail::cook(g_frozen, &j));
  EXPECT_EQ(j["warren"], "simple");
  std::map<std::string, std::string> k;
  EXPECT_TRUE(cottontail::cook(j["idx"], &k));
  EXPECT_EQ(k["name"], "simple");
  std::map<std::string, std::string> l;
  EXPECT_TRUE(cottontail::cook(k["recipe"], &l));
  EXPECT_EQ(l["fvalue_compressor"], "zlib");
  EXPECT_EQ(l["posting_compressor_recipe"], "");
}

TEST(recipe, Options) {
  std::string dna = "["
                    "  featurizer:["
                    "    name:\"hashing\","
                    "    recipe:\"\","
                    "  ],"
                    "  idx:["
                    "    name:\"simple\","
                    "    recipe:["
                    "      fvalue_compressor:\"zlib\","
                    "      fvalue_compressor_recipe:\"\","
                    "      posting_compressor:\"post\","
                    "      posting_compressor_recipe:\"\","
                    "    ],"
                    "  ],"
                    "  tokenizer:["
                    "    name:\"ascii\","
                    "    recipe:\"xml\","
                    "  ],"
                    "  txt:["
                    "    name:\"simple\","
                    "    recipe:["
                    "      compressor:\"zlib\","
                    "      compressor_recipe:\"\","
                    "    ],"
                    "  ],"
                    "  warren:\"simple\","
                    "]";
  std::string option, value;
  option = "tokenizer:noxml";
  EXPECT_TRUE(cottontail::interpret_option(&dna, option));
  option = "tokenizer";
  EXPECT_TRUE(cottontail::extract_option(dna, option, &value));
  EXPECT_EQ(value, "noxml");
  option = "tokenizer:name:utf8";
  EXPECT_TRUE(cottontail::interpret_option(&dna, option));
  option = "tokenizer:name";
  EXPECT_TRUE(cottontail::extract_option(dna, option, &value));
  EXPECT_EQ(value, "utf8");
  option = "idx:fvalue_compressor:null";
  EXPECT_TRUE(cottontail::interpret_option(&dna, option));
  option = "idx:fvalue_compressor";
  EXPECT_TRUE(cottontail::extract_option(dna, option, &value));
  EXPECT_EQ(value, "null");
  std::string bad = "bad";
  EXPECT_FALSE(cottontail::interpret_option(&bad, option));
  EXPECT_FALSE(cottontail::extract_option(bad, option, &value));
  option = "a:very:long:option";
  EXPECT_FALSE(cottontail::interpret_option(&dna, option));
  EXPECT_FALSE(cottontail::extract_option(dna, option, &value));
  option = "thing:thing";
  EXPECT_FALSE(cottontail::interpret_option(&dna, option));
  option = "thing:name:foo";
  EXPECT_FALSE(cottontail::interpret_option(&dna, option));
}

TEST(recipe, Wrap) {
  std::string dna = "[\n"
                    "  featurizer:[\n"
                    "    name:\"hashing\",\n"
                    "    recipe:\"\",\n"
                    "  ],\n"
                    "  idx:[\n"
                    "    name:\"bigwig\",\n"
                    "    recipe:[\n"
                    "      fvalue_compressor:\"null\",\n"
                    "      fvalue_compressor_recipe:\"\",\n"
                    "      posting_compressor:\"null\",\n"
                    "      posting_compressor_recipe:\"\",\n"
                    "    ],\n"
                    "  ],\n"
                    "  parameters:[\n"
                    "    container:\"(... <DOC> </DOC>)\",\n"
                    "    id:\"(... <DOCNO> </DOCNO>)\",\n"
                    "    statistics:\"tf\",\n"
                    "    stemmer:\"porter\",\n"
                    "  ],\n"
                    "  tokenizer:[\n"
                    "    name:\"ascii\",\n"
                    "    recipe:\"xml\",\n"
                    "  ],\n"
                    "  txt:[\n"
                    "    name:\"bigwig\",\n"
                    "    recipe:[\n"
                    "      compressor:\"null\",\n"
                    "      compressor_recipe:\"\",\n"
                    "    ],\n"
                    "  ],\n"
                    "  warren:\"bigwig\",\n"
                    "]\n";
  EXPECT_TRUE(cottontail::interpret_option(&dna, "featurizer@json"));
  EXPECT_TRUE(cottontail::interpret_option(&dna, "txt@json"));
  std::string value;
  EXPECT_TRUE(cottontail::extract_option(dna, "txt:name", &value));
  EXPECT_EQ(value, "json");
  EXPECT_TRUE(cottontail::extract_option(dna, "idx:name", &value));
  EXPECT_EQ(value, "bigwig");
  EXPECT_TRUE(cottontail::extract_option(dna, "featurizer:name", &value));
  EXPECT_EQ(value, "json");
  std::map<std::string, std::string> parameters;
  EXPECT_TRUE(cottontail::cook(dna, &parameters));
  EXPECT_TRUE(cottontail::cook(parameters["featurizer"], &parameters));
  std::string name, recipe;
  EXPECT_TRUE(cottontail::unwrap(parameters["recipe"], &name, &recipe));
  EXPECT_EQ(name, "hashing");
  EXPECT_EQ(recipe, "");
  EXPECT_FALSE(cottontail::interpret_option(&dna, "junk@json"));
  EXPECT_FALSE(cottontail::interpret_option(&dna, "x@y@z"));
}
