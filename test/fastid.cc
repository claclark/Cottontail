#include <memory>
#include <string>

#include "gtest/gtest.h"

#include "src/cottontail.h"

TEST(Fastid, Basic) {
  std::string error;
  std::string burrow = cottontail::DEFAULT_BURROW;
  {
    std::string options = "";
    std::shared_ptr<cottontail::Working> working =
        cottontail::Working::mkdir(burrow);
    ASSERT_NE(working, nullptr);
    std::shared_ptr<cottontail::Builder> builder =
        cottontail::SimpleBuilder::make(working, options, &error);
    ASSERT_NE(builder, nullptr);
    std::vector<std::string> text;
    text.push_back("test/ranking.txt");
    ASSERT_TRUE(cottontail::build_trec(text, builder, &error));
  }
  std::string simple = "simple";
  std::shared_ptr<cottontail::Warren> warren;
  {
    std::string container = "(... <DOC> </DOC>)";
    std::string id_key = "id";
    std::string id_query = "(... <DOCNO> </DOCNO>)";
    warren = cottontail::Warren::make(simple, burrow, &error);
    ASSERT_NE(warren, nullptr);
    warren->start();
    warren->set_default_container(container);
    warren->set_parameter(id_key, id_query, &error);
    ASSERT_TRUE(tf_idf_annotations(warren, &error));
    warren->end();
  }
  {
    warren = cottontail::Warren::make(simple, burrow, &error);
    ASSERT_NE(warren, nullptr);
    warren->start();
    ASSERT_TRUE(cottontail::fastid(warren, &error));
    warren->end();
  }
  {
    warren = cottontail::Warren::make(simple, burrow, &error);
    warren->start();
    cottontail::addr p, q;
    std::string target0 = "<DOCNO> doc-002 </DOCNO>\n";
    std::string gcl0 = "\"" + target0 + "\"";
    std::unique_ptr<cottontail::Hopper> hopper0 =
        warren->hopper_from_gcl(gcl0, &error);
    ASSERT_NE(hopper0, nullptr);
    hopper0->tau(cottontail::minfinity + 1, &p, &q);
    std::string actual0 = warren->txt()->translate(p, q);
    ASSERT_EQ(target0, actual0);
    std::string target1 = "the cat in the hat ";
    std::string gcl1 = "\"" + target1 + "\"";
    std::unique_ptr<cottontail::Hopper> hopper1 =
        warren->hopper_from_gcl(gcl1, &error);
    ASSERT_NE(hopper1, nullptr);
    hopper1->tau(cottontail::minfinity + 1, &p, &q);
    std::string actual1 = warren->txt()->translate(p, q);
    ASSERT_EQ(target1, actual1);
    warren->end();
  }
}
