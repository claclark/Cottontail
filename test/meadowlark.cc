#include <memory>
#include <string>

#include "gtest/gtest.h"

#include "meadowlark/meadowlark.h"
#include "src/cottontail.h"

TEST(Meadowlark, JSONMetadata) {
  const std::string path = "test/books.json";
  std::string error;
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::meadowlark::create_meadow("json.meadow", &error);
  ASSERT_NE(warren, nullptr) << error;
  ASSERT_TRUE(cottontail::meadowlark::append_jsonl(
      warren, path, &error, 1, false))
      << error;

  warren->start();
  std::shared_ptr<cottontail::Hopper> metadata =
      warren->idx()->hopper(warren->featurizer()->featurize("@"));
  ASSERT_NE(metadata, nullptr);
  cottontail::addr p, q;
  metadata->tau(cottontail::minfinity + 1, &p, &q);
  ASSERT_NE(p, cottontail::maxfinity);
  const std::string text =
      cottontail::json_translate(warren->txt()->translate(p, q));
  EXPECT_NE(text.find("\"type\": \"json\""), std::string::npos);
  EXPECT_NE(text.find("\"file\": \"test/books.json\""), std::string::npos);

  std::unique_ptr<cottontail::Hopper> typed =
      warren->hopper_from_gcl("(>> @ (>> :type: \"json\"))", &error);
  ASSERT_NE(typed, nullptr) << error;
  cottontail::addr typed_p, typed_q;
  typed->tau(cottontail::minfinity + 1, &typed_p, &typed_q);
  EXPECT_EQ(typed_p, p);
  EXPECT_EQ(typed_q, q);

  std::unique_ptr<cottontail::Hopper> described = warren->hopper_from_gcl(
      "(>> (>> @ (>> :type: \"json\")) "
      "(>> :file: \"test/books.json\"))",
      &error);
  ASSERT_NE(described, nullptr) << error;
  cottontail::addr described_p, described_q;
  described->tau(cottontail::minfinity + 1, &described_p, &described_q);
  EXPECT_EQ(described_p, p);
  EXPECT_EQ(described_q, q);

  std::unique_ptr<cottontail::Hopper> metadata_source =
      warren->hopper_from_gcl("(<< // (>> /. @))", &error);
  ASSERT_NE(metadata_source, nullptr) << error;
  cottontail::addr metadata_name_p, metadata_name_q;
  metadata_source->tau(cottontail::minfinity + 1, &metadata_name_p,
                       &metadata_name_q);
  ASSERT_NE(metadata_name_p, cottontail::maxfinity);
  EXPECT_EQ(warren->txt()
                ->translate(metadata_name_p, metadata_name_q)
                .substr(0, path.size()),
            path);

  std::unique_ptr<cottontail::Hopper> data_source =
      warren->hopper_from_gcl("(<< // (>> /. :))", &error);
  ASSERT_NE(data_source, nullptr) << error;
  cottontail::addr data_name_p, data_name_q;
  data_source->tau(cottontail::minfinity + 1, &data_name_p, &data_name_q);
  ASSERT_NE(data_name_p, cottontail::maxfinity);
  EXPECT_EQ(warren->txt()
                ->translate(data_name_p, data_name_q)
                .substr(0, path.size()),
            path);

  std::shared_ptr<cottontail::Hopper> sources =
      warren->idx()->hopper(warren->featurizer()->featurize("/"));
  ASSERT_NE(sources, nullptr);
  cottontail::addr canonical_p, canonical_q;
  sources->tau(cottontail::minfinity + 1, &canonical_p, &canonical_q);
  ASSERT_EQ(canonical_p, metadata_name_p);
  EXPECT_EQ(canonical_q, metadata_name_q);
  sources->tau(canonical_p + 1, &canonical_p, &canonical_q);
  EXPECT_EQ(canonical_p, cottontail::maxfinity);

  std::shared_ptr<cottontail::Hopper> file =
      warren->idx()->hopper(warren->featurizer()->featurize(path));
  ASSERT_NE(file, nullptr);
  cottontail::addr file_p, file_q;
  file->tau(p, &file_p, &file_q);
  EXPECT_NE(file_p, p);
  EXPECT_NE(file_p, cottontail::maxfinity);

  std::shared_ptr<cottontail::Hopper> objects =
      warren->idx()->hopper(warren->featurizer()->featurize(":"));
  ASSERT_NE(objects, nullptr);
  cottontail::addr object_p, object_q;
  objects->tau(p, &object_p, &object_q);
  EXPECT_NE(object_p, p);
  EXPECT_EQ(object_p, file_p);
  EXPECT_EQ(object_q, file_q);
  warren->end();
}

TEST(Meadowlark, TSV) {
  const std::string path = "test/test.tsv";
  std::string error;
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::meadowlark::create_meadow("tsv.meadow", &error);
  ASSERT_NE(warren, nullptr) << error;
  ASSERT_TRUE(cottontail::meadowlark::append_tsv(warren, "test/test.tsv",
                                                 &error, true, "\t", 1))
      << error;
  warren->start();
  std::unique_ptr<cottontail::Hopper> metadata =
      warren->hopper_from_gcl("(>> @ (>> :type: \"tsv\"))", &error);
  ASSERT_NE(metadata, nullptr) << error;
  cottontail::addr metadata_p, metadata_q;
  metadata->tau(cottontail::minfinity + 1, &metadata_p, &metadata_q);
  ASSERT_NE(metadata_p, cottontail::maxfinity);
  std::string description =
      cottontail::json_translate(warren->txt()->translate(metadata_p,
                                                          metadata_q));
  EXPECT_NE(description.find("\"file\": \"test/test.tsv\""),
            std::string::npos);
  EXPECT_NE(description.find("\"separator\": \"\\t\""),
            std::string::npos);
  EXPECT_NE(description.find("\"header\": true"), std::string::npos);
  EXPECT_NE(description.find("\"header\": \"Favorite Food\""),
            std::string::npos);
  EXPECT_NE(description.find("\"feature\": \":Favorite_Food:\""),
            std::string::npos);

  std::unique_ptr<cottontail::Hopper> described = warren->hopper_from_gcl(
      "(>> (>> @ (>> :type: \"tsv\")) "
      "(>> :file: \"test/test.tsv\"))",
      &error);
  ASSERT_NE(described, nullptr) << error;
  cottontail::addr described_p, described_q;
  described->tau(cottontail::minfinity + 1, &described_p, &described_q);
  EXPECT_EQ(described_p, metadata_p);
  EXPECT_EQ(described_q, metadata_q);

  std::unique_ptr<cottontail::Hopper> metadata_source =
      warren->hopper_from_gcl("(<< // (>> /. @))", &error);
  ASSERT_NE(metadata_source, nullptr) << error;
  cottontail::addr source_p, source_q;
  metadata_source->tau(cottontail::minfinity + 1, &source_p, &source_q);
  ASSERT_NE(source_p, cottontail::maxfinity);
  EXPECT_EQ(warren->txt()->translate(source_p, source_q).substr(0, path.size()),
            path);

  std::unique_ptr<cottontail::Hopper> data_source =
      warren->hopper_from_gcl("(<< // (>> /. :))", &error);
  ASSERT_NE(data_source, nullptr) << error;
  data_source->tau(cottontail::minfinity + 1, &source_p, &source_q);
  ASSERT_NE(source_p, cottontail::maxfinity);
  EXPECT_EQ(warren->txt()->translate(source_p, source_q).substr(0, path.size()),
            path);

  std::shared_ptr<cottontail::Hopper> file =
      warren->idx()->hopper(warren->featurizer()->featurize(path));
  ASSERT_NE(file, nullptr);
  cottontail::addr file_p, file_q;
  file->tau(metadata_p, &file_p, &file_q);
  EXPECT_NE(file_p, metadata_p);
  EXPECT_NE(file_p, cottontail::maxfinity);

  std::shared_ptr<cottontail::Hopper> hopper =
      warren->hopper_from_gcl(
          "(<< :Animal: (>> : (>> :Hobby: \"Mud bath\")))");
  ASSERT_NE(hopper, nullptr);
  cottontail::addr p, q;
  hopper->rho(0, &p, &q);
  std::string pig = warren->txt()->translate(p, q).substr(0, 3);
  EXPECT_EQ(pig, "Pig");
  hopper = warren->hopper_from_gcl(
      "(<< :Favorite_Food: (>> : (>> :Animal: \"Owl\")))");
  ASSERT_NE(hopper, nullptr);
  hopper->ohr(10000, &p, &q);
  std::string mouse = warren->txt()->translate(p, q).substr(0, 5);
  EXPECT_EQ(mouse, "Mouse");
  warren->end();
}

TEST(Meadowlark, TSVWithoutHeaderUsesNumericColumns) {
  const std::string path = "test/test.tsv";
  std::string error;
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::meadowlark::create_meadow("numeric-tsv.meadow", &error);
  ASSERT_NE(warren, nullptr) << error;
  ASSERT_TRUE(cottontail::meadowlark::append_tsv(warren, path, &error, false,
                                                 "\t", 1))
      << error;
  warren->start();
  std::unique_ptr<cottontail::Hopper> metadata =
      warren->hopper_from_gcl("(>> @ (>> :type: \"tsv\"))", &error);
  ASSERT_NE(metadata, nullptr) << error;
  cottontail::addr metadata_p, metadata_q;
  metadata->tau(cottontail::minfinity + 1, &metadata_p, &metadata_q);
  ASSERT_NE(metadata_p, cottontail::maxfinity);
  std::string description =
      cottontail::json_translate(warren->txt()->translate(metadata_p,
                                                          metadata_q));
  EXPECT_NE(description.find("\"header\": false"), std::string::npos);
  EXPECT_NE(description.find("\"feature\": \":0:\""),
            std::string::npos);
  EXPECT_NE(description.find("\"feature\": \":1:\""),
            std::string::npos);

  std::unique_ptr<cottontail::Hopper> hopper = warren->hopper_from_gcl(
      "(<< :0: (>> : (>> :3: \"Mud bath\")))", &error);
  ASSERT_NE(hopper, nullptr) << error;
  cottontail::addr p, q;
  hopper->rho(0, &p, &q);
  EXPECT_EQ(warren->txt()->translate(p, q).substr(0, 3), "Pig");
  warren->end();
}
