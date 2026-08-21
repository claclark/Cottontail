#include <memory>
#include <string>
#include <vector>

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

  std::unique_ptr<cottontail::Hopper> metadata_container =
      warren->hopper_from_gcl("(>> /. @)", &error);
  ASSERT_NE(metadata_container, nullptr) << error;
  cottontail::addr metadata_container_p, metadata_container_q;
  metadata_container->tau(cottontail::minfinity + 1, &metadata_container_p,
                          &metadata_container_q);
  EXPECT_EQ(metadata_container_p, cottontail::maxfinity);

  std::unique_ptr<cottontail::Hopper> data_source =
      warren->hopper_from_gcl("(<< // (>> /. :))", &error);
  ASSERT_NE(data_source, nullptr) << error;
  cottontail::addr data_name_p, data_name_q;
  data_source->tau(cottontail::minfinity + 1, &data_name_p, &data_name_q);
  ASSERT_NE(data_name_p, cottontail::maxfinity);
  EXPECT_EQ(cottontail::json_translate(
                warren->txt()->translate(data_name_p, data_name_q))
                .substr(0, path.size() + 2),
            "\"" + path + "\"");

  std::shared_ptr<cottontail::Hopper> sources =
      warren->idx()->hopper(warren->featurizer()->featurize("/"));
  ASSERT_NE(sources, nullptr);
  cottontail::addr canonical_p, canonical_q;
  sources->tau(cottontail::minfinity + 1, &canonical_p, &canonical_q);
  ASSERT_NE(canonical_p, cottontail::maxfinity);
  EXPECT_EQ(cottontail::json_translate(
                warren->txt()->translate(canonical_p, canonical_q))
                .substr(0, path.size() + 2),
            "\"" + path + "\"");
  EXPECT_NE(canonical_p, data_name_p);
  sources->tau(canonical_p + 1, &canonical_p, &canonical_q);
  EXPECT_EQ(canonical_p, cottontail::maxfinity);

  std::shared_ptr<cottontail::Hopper> file =
      warren->idx()->hopper(warren->featurizer()->featurize(path));
  ASSERT_NE(file, nullptr);
  cottontail::addr file_p, file_q;
  file->tau(p, &file_p, &file_q);
  EXPECT_NE(file_p, p);
  EXPECT_NE(file_p, cottontail::maxfinity);
  file->tau(file_p + 1, &p, &q);
  EXPECT_EQ(p, cottontail::maxfinity);

  std::shared_ptr<cottontail::Hopper> objects =
      warren->idx()->hopper(warren->featurizer()->featurize(":"));
  ASSERT_NE(objects, nullptr);
  cottontail::addr object_p, object_q;
  objects->tau(cottontail::minfinity + 1, &object_p, &object_q);
  EXPECT_NE(object_p, cottontail::maxfinity);
  EXPECT_LE(file_p, object_p);
  EXPECT_GE(file_q, object_q);

  std::unique_ptr<cottontail::Hopper> named_objects =
      warren->hopper_from_gcl("(<< : test/books.json)", &error);
  ASSERT_NE(named_objects, nullptr) << error;
  named_objects->tau(cottontail::minfinity + 1, &p, &q);
  EXPECT_EQ(p, object_p);
  EXPECT_EQ(q, object_q);
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

  std::unique_ptr<cottontail::Hopper> metadata_container =
      warren->hopper_from_gcl("(>> /. @)", &error);
  ASSERT_NE(metadata_container, nullptr) << error;
  cottontail::addr source_p, source_q;
  metadata_container->tau(cottontail::minfinity + 1, &source_p, &source_q);
  EXPECT_EQ(source_p, cottontail::maxfinity);

  std::unique_ptr<cottontail::Hopper> data_source =
      warren->hopper_from_gcl("(<< // (>> /. :))", &error);
  ASSERT_NE(data_source, nullptr) << error;
  data_source->tau(cottontail::minfinity + 1, &source_p, &source_q);
  ASSERT_NE(source_p, cottontail::maxfinity);
  EXPECT_EQ(cottontail::json_translate(
                warren->txt()->translate(source_p, source_q))
                .substr(0, path.size() + 2),
            "\"" + path + "\"");

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

TEST(Meadowlark, TextFileIsOneObject) {
  const std::string path = "test/code.txt";
  std::string error;
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::meadowlark::create_meadow("text.meadow", &error);
  ASSERT_NE(warren, nullptr) << error;
  ASSERT_TRUE(cottontail::meadowlark::append_text(warren, path, &error))
      << error;

  warren->start();
  std::unique_ptr<cottontail::Hopper> metadata =
      warren->hopper_from_gcl("(>> @ (>> :type: \"text\"))", &error);
  ASSERT_NE(metadata, nullptr) << error;
  cottontail::addr metadata_p, metadata_q;
  metadata->tau(cottontail::minfinity + 1, &metadata_p, &metadata_q);
  ASSERT_NE(metadata_p, cottontail::maxfinity);

  std::shared_ptr<cottontail::Hopper> objects =
      warren->idx()->hopper(warren->featurizer()->featurize(":"));
  ASSERT_NE(objects, nullptr);
  cottontail::addr object_p, object_q;
  objects->tau(cottontail::minfinity + 1, &object_p, &object_q);
  ASSERT_NE(object_p, cottontail::maxfinity);
  std::string object = warren->txt()->translate(object_p, object_q);
  EXPECT_NE(object.find("alpha beta"), std::string::npos);
  EXPECT_NE(object.find("charlie delta"), std::string::npos);
  objects->tau(object_p + 1, &object_p, &object_q);
  EXPECT_EQ(object_p, cottontail::maxfinity);

  std::unique_ptr<cottontail::Hopper> metadata_container =
      warren->hopper_from_gcl("(>> /. @)", &error);
  ASSERT_NE(metadata_container, nullptr) << error;
  cottontail::addr p, q;
  metadata_container->tau(cottontail::minfinity + 1, &p, &q);
  EXPECT_EQ(p, cottontail::maxfinity);

  std::unique_ptr<cottontail::Hopper> data_source =
      warren->hopper_from_gcl("(<< // (>> /. :))", &error);
  ASSERT_NE(data_source, nullptr) << error;
  data_source->tau(cottontail::minfinity + 1, &p, &q);
  ASSERT_NE(p, cottontail::maxfinity);
  EXPECT_EQ(cottontail::json_translate(warren->txt()->translate(p, q))
                .substr(0, path.size() + 2),
            "\"" + path + "\"");
  warren->end();
}

TEST(Meadowlark, CodeLinesCarryPhysicalLineNumbers) {
  const std::string path = "test/code.txt";
  std::string error;
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::meadowlark::create_meadow("code.meadow", &error);
  ASSERT_NE(warren, nullptr) << error;
  ASSERT_TRUE(cottontail::meadowlark::append_code(warren, path, &error))
      << error;

  warren->start();
  std::unique_ptr<cottontail::Hopper> metadata =
      warren->hopper_from_gcl("(>> @ (>> :type: \"code\"))", &error);
  ASSERT_NE(metadata, nullptr) << error;
  cottontail::addr p, q;
  metadata->tau(cottontail::minfinity + 1, &p, &q);
  ASSERT_NE(p, cottontail::maxfinity);

  std::shared_ptr<cottontail::Hopper> lines =
      warren->idx()->hopper(warren->featurizer()->featurize("#"));
  ASSERT_NE(lines, nullptr);
  cottontail::addr line_number;
  lines->tau(cottontail::minfinity + 1, &p, &q, &line_number);
  ASSERT_NE(p, cottontail::maxfinity);
  EXPECT_EQ(line_number, 1);
  EXPECT_NE(warren->txt()->translate(p, q).find("alpha beta"),
            std::string::npos);
  lines->tau(p + 1, &p, &q, &line_number);
  ASSERT_NE(p, cottontail::maxfinity);
  EXPECT_EQ(line_number, 3);
  EXPECT_NE(warren->txt()->translate(p, q).find("charlie delta"),
            std::string::npos);
  lines->tau(p + 1, &p, &q, &line_number);
  EXPECT_EQ(p, cottontail::maxfinity);

  std::unique_ptr<cottontail::Hopper> source =
      warren->hopper_from_gcl("(<< // (>> /. #))", &error);
  ASSERT_NE(source, nullptr) << error;
  source->tau(cottontail::minfinity + 1, &p, &q);
  ASSERT_NE(p, cottontail::maxfinity);
  EXPECT_EQ(cottontail::json_translate(warren->txt()->translate(p, q))
                .substr(0, path.size() + 2),
            "\"" + path + "\"");
  warren->end();
}

TEST(Meadowlark, TokenlessFileHasIdentityMetadataAndLocalNameOnly) {
  const std::string path = "./test/tokenless.txt";
  std::string error;
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::meadowlark::create_meadow("tokenless.meadow", &error);
  ASSERT_NE(warren, nullptr) << error;
  ASSERT_TRUE(cottontail::meadowlark::append_text(warren, path, &error))
      << error;

  warren->start();
  auto count = [&](const std::string &feature) {
    std::shared_ptr<cottontail::Hopper> hopper =
        warren->idx()->hopper(warren->featurizer()->featurize(feature));
    size_t n = 0;
    cottontail::addr p, q;
    for (hopper->tau(cottontail::minfinity + 1, &p, &q);
         p < cottontail::maxfinity; hopper->tau(p + 1, &p, &q))
      n++;
    return n;
  };
  EXPECT_EQ(count("/"), size_t{1});
  EXPECT_EQ(count("@"), size_t{1});
  EXPECT_EQ(count("//"), size_t{1});
  EXPECT_EQ(count("/."), size_t{0});
  EXPECT_EQ(count(":"), size_t{0});
  EXPECT_EQ(count(path), size_t{0});

  std::shared_ptr<cottontail::Hopper> names =
      warren->idx()->hopper(warren->featurizer()->featurize("/"));
  cottontail::addr p, q;
  names->tau(cottontail::minfinity + 1, &p, &q);
  ASSERT_NE(p, cottontail::maxfinity);
  EXPECT_EQ(cottontail::json_translate(warren->txt()->translate(p, q))
                .substr(0, path.size() + 2),
            "\"" + path + "\"");

  bool appended = false;
  ASSERT_TRUE(cottontail::meadowlark::already_appended(
      warren, path, &appended, &error))
      << error;
  EXPECT_TRUE(appended);
  warren->end();
}

TEST(Meadowlark, RestartRecognizesLegacyRawFilename) {
  const std::string path = "./legacy.txt";
  std::string error;
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::meadowlark::create_meadow("legacy-name.meadow", &error);
  ASSERT_NE(warren, nullptr) << error;
  warren->start();
  ASSERT_TRUE(warren->transaction(&error)) << error;
  cottontail::addr p, q;
  ASSERT_TRUE(warren->appender()->append(path, &p, &q, &error)) << error;
  ASSERT_TRUE(warren->annotator()->annotate(
      warren->featurizer()->featurize("/"), p, q, &error))
      << error;
  ASSERT_TRUE(warren->ready(&error)) << error;
  warren->commit();
  warren->end();
  warren->start();

  bool appended = false;
  ASSERT_TRUE(cottontail::meadowlark::already_appended(
      warren, "legacy.txt", &appended, &error))
      << error;
  EXPECT_TRUE(appended);
  warren->end();
}

TEST(Meadowlark, AppendPlanMixesTextAndCode) {
  std::string error;
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::meadowlark::create_meadow("append-plan.meadow", &error);
  ASSERT_NE(warren, nullptr) << error;
  std::vector<cottontail::meadowlark::InputFile> files = {
      {cottontail::meadowlark::InputType::TEXT, "test/sonnet0.txt"},
      {cottontail::meadowlark::InputType::JSONL, "test/books.json"},
      {cottontail::meadowlark::InputType::CODE, "test/code.txt"},
      {cottontail::meadowlark::InputType::TSV, "test/test.tsv"},
      {cottontail::meadowlark::InputType::TEXT, "test/sonnet1.txt"},
  };
  ASSERT_TRUE(
      cottontail::meadowlark::append_all(warren, files, &error, 2, false))
      << error;
  ASSERT_TRUE(
      cottontail::meadowlark::append_all(warren, files, &error, 2, false))
      << error;

  warren->start();
  std::shared_ptr<cottontail::Hopper> sources =
      warren->idx()->hopper(warren->featurizer()->featurize("/"));
  ASSERT_NE(sources, nullptr);
  cottontail::addr p, q;
  size_t count = 0;
  for (sources->tau(cottontail::minfinity + 1, &p, &q);
       p < cottontail::maxfinity; sources->tau(p + 1, &p, &q))
    count++;
  EXPECT_EQ(count, files.size());
  for (const std::string &type :
       std::vector<std::string>{"text", "json", "code", "tsv"}) {
    std::unique_ptr<cottontail::Hopper> metadata = warren->hopper_from_gcl(
        "(>> @ (>> :type: \"" + type + "\"))", &error);
    ASSERT_NE(metadata, nullptr) << error;
    metadata->tau(cottontail::minfinity + 1, &p, &q);
    EXPECT_NE(p, cottontail::maxfinity) << type;
  }
  warren->end();
}
