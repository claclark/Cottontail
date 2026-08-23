#include <map>
#include <memory>
#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "meadowlark/meadowlark.h"
#include "meadowlark/metadata.h"
#include "meadowlark/tf-idf_forager.h"
#include "meadowlark/tf-idf_stats.h"
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
  EXPECT_NE(text.find("\"filename\": \"test/books.json\""),
            std::string::npos);

  std::unique_ptr<cottontail::Hopper> typed =
      warren->hopper_from_gcl("(>> @ (>> :type: \"json\"))", &error);
  ASSERT_NE(typed, nullptr) << error;
  cottontail::addr typed_p, typed_q;
  typed->tau(cottontail::minfinity + 1, &typed_p, &typed_q);
  EXPECT_EQ(typed_p, p);
  EXPECT_EQ(typed_q, q);

  std::unique_ptr<cottontail::Hopper> described = warren->hopper_from_gcl(
      "(>> (>> @ (>> :type: \"json\")) "
      "(>> :filename: \"test/books.json\"))",
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
  EXPECT_NE(description.find("\"filename\": \"test/test.tsv\""),
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
      "(>> :filename: \"test/test.tsv\"))",
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

TEST(Meadowlark, FileOrientedTfIdfForage) {
  const std::string path = "test/code.txt";
  const std::string tag = "file-oriented";
  std::string error;
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::meadowlark::create_meadow("forage.meadow", &error);
  ASSERT_NE(warren, nullptr) << error;
  ASSERT_TRUE(cottontail::meadowlark::append_text(warren, path, &error))
      << error;
  std::map<std::string, std::string> reserved = {{"filename", path}};
  EXPECT_FALSE(cottontail::meadowlark::forage(
      warren, path, ":", "tf-idf", "reserved", reserved, &error, 1));
  std::map<std::string, std::string> historical = {{"contents", ":"}};
  EXPECT_FALSE(cottontail::meadowlark::forage(
      warren, path, ":", "tf-idf", "historical", historical, &error, 1));
  std::map<std::string, std::string> parameters = {
      {"container", "/."}, {"stemmer", "porter"}};
  ASSERT_TRUE(cottontail::meadowlark::forage(
      warren, path, ":", "tf-idf", tag, parameters, &error, 1))
      << error;
  ASSERT_TRUE(cottontail::meadowlark::forage(
      warren, path, ":", "tf-idf", tag, &error, 1))
      << error;

  warren->start();
  bool foraged = false;
  ASSERT_TRUE(cottontail::meadowlark::already_foraged(
      warren, path, "tf-idf", tag, &foraged, &error))
      << error;
  EXPECT_TRUE(foraged);
  EXPECT_NE(cottontail::meadowlark::TfIdfForager::make(warren, tag, &error),
            nullptr)
      << error;
  EXPECT_NE(cottontail::meadowlark::TfIdfStats::make(tag, warren, &error),
            nullptr)
      << error;

  std::unique_ptr<cottontail::Hopper> primary = warren->hopper_from_gcl(
      "(>> (>> (>> (>> @ (>> :type: \"forager\")) "
      "(>> :name: \"tf-idf\")) (>> :tag: \"file-oriented\")) :query:)",
      &error);
  ASSERT_NE(primary, nullptr) << error;
  cottontail::addr p, q;
  primary->tau(cottontail::minfinity + 1, &p, &q);
  ASSERT_NE(p, cottontail::maxfinity);
  cottontail::meadowlark::ForagerMetadata metadata;
  ASSERT_TRUE(cottontail::meadowlark::json2forager(
      warren->txt()->translate(p, q), &metadata, &error))
      << error;
  EXPECT_TRUE(metadata.has_query);
  EXPECT_FALSE(metadata.has_filename);
  EXPECT_EQ(metadata.query, ":");
  warren->end();
}

TEST(Meadowlark, ForageAllUsesMeadowSelectors) {
  std::string error;
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::meadowlark::create_meadow("forage-all.meadow", &error);
  ASSERT_NE(warren, nullptr) << error;
  std::vector<cottontail::meadowlark::InputFile> files = {
      {cottontail::meadowlark::InputType::TEXT, "test/sonnet0.txt"},
      {cottontail::meadowlark::InputType::TEXT, "test/sonnet1.txt"},
  };
  ASSERT_TRUE(
      cottontail::meadowlark::append_all(warren, files, &error, 2, false))
      << error;
  std::map<std::string, std::string> no_parameters;
  ASSERT_TRUE(cottontail::meadowlark::forage_all(
      warren, {"test/sonnet*.txt"}, ":", "null", "pattern", no_parameters,
      &error, 2))
      << error;

  warren->start();
  for (const auto &file : files) {
    bool foraged = false;
    ASSERT_TRUE(cottontail::meadowlark::already_foraged(
        warren, file.filename, "null", "pattern", &foraged, &error))
        << error;
    EXPECT_TRUE(foraged) << file.filename;
  }
  warren->end();

  EXPECT_FALSE(cottontail::meadowlark::forage_all(
      warren, {"missing*.txt"}, ":", "null", "pattern", &error, 2));
  ASSERT_TRUE(cottontail::meadowlark::forage_all(
      warren, {}, ":", "null", "all", no_parameters, &error, 2))
      << error;
}

TEST(Meadowlark, ExplicitZeroResultForageWritesCompletion) {
  const std::string path = "test/code.txt";
  std::string error;
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::meadowlark::create_meadow("zero-forage.meadow", &error);
  ASSERT_NE(warren, nullptr) << error;
  ASSERT_TRUE(cottontail::meadowlark::append_text(warren, path, &error))
      << error;
  std::map<std::string, std::string> no_parameters;
  ASSERT_TRUE(cottontail::meadowlark::forage(
      warren, path, ":not-present:", "null", "zero", no_parameters, &error,
      1))
      << error;
  warren->start();
  bool foraged = false;
  ASSERT_TRUE(cottontail::meadowlark::already_foraged(
      warren, path, "null", "zero", &foraged, &error))
      << error;
  EXPECT_TRUE(foraged);
  warren->end();
}

TEST(Meadowlark, RefusesLegacyForagerDefinition) {
  const std::string path = "test/code.txt";
  std::string error;
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::meadowlark::create_meadow("legacy-forage.meadow", &error);
  ASSERT_NE(warren, nullptr) << error;
  ASSERT_TRUE(cottontail::meadowlark::append_text(warren, path, &error))
      << error;
  warren->start();
  ASSERT_TRUE(warren->transaction(&error)) << error;
  cottontail::addr p, q;
  ASSERT_TRUE(cottontail::json_append(
      "{\"type\":\"forager\",\"name\":\"tf-idf\","
      "\"tag\":\"old\",\"parameters\":{\"contents\":\":\","
      "\"start\":\"0\",\"end\":\"1\"}}",
      warren, &p, &q, "@", &error))
      << error;
  ASSERT_TRUE(warren->ready(&error)) << error;
  warren->commit();
  warren->end();

  std::map<std::string, std::string> parameters = {{"container", ":"}};
  EXPECT_FALSE(cottontail::meadowlark::forage(
      warren, path, ":", "tf-idf", "old", parameters, &error, 1));
  EXPECT_NE(error.find("historical interval metadata"), std::string::npos);
}

TEST(Meadowlark, ForagerDefinitionIsImmutable) {
  const std::string path = "test/code.txt";
  const std::string tag = "immutable";
  std::string error;
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::meadowlark::create_meadow("immutable-forage.meadow", &error);
  ASSERT_NE(warren, nullptr) << error;
  ASSERT_TRUE(cottontail::meadowlark::append_text(warren, path, &error))
      << error;

  std::map<std::string, std::string> parameters = {
      {"container", "/."}, {"stemmer", "porter"}};
  ASSERT_TRUE(cottontail::meadowlark::forage(
      warren, path, ":", "tf-idf", tag, parameters, &error, 1))
      << error;

  std::map<std::string, std::string> changed = {
      {"container", ":"}, {"stemmer", "porter"}};
  error.clear();
  EXPECT_FALSE(cottontail::meadowlark::forage(
      warren, path, ":", "tf-idf", tag, changed, &error, 1));
  EXPECT_NE(error.find("specification does not match"), std::string::npos);

  error.clear();
  EXPECT_FALSE(cottontail::meadowlark::forage(
      warren, path, "#", "tf-idf", tag, &error, 1));
  EXPECT_NE(error.find("query does not match"), std::string::npos);

  error.clear();
  ASSERT_TRUE(cottontail::meadowlark::forage(
      warren, path, ":", "tf-idf", tag, &error, 1))
      << error;

  warren->start();
  EXPECT_NE(cottontail::meadowlark::TfIdfForager::make(warren, tag, &error),
            nullptr)
      << error;
  error.clear();
  EXPECT_EQ(cottontail::meadowlark::TfIdfForager::make(warren, "missing",
                                                       &error),
            nullptr);
  EXPECT_NE(error.find("No current forager definition"), std::string::npos);
  warren->end();
}

TEST(Meadowlark, InvalidForagerSpecificationDoesNotPublishDefinition) {
  const std::string path = "test/code.txt";
  std::string error;
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::meadowlark::create_meadow("invalid-forage.meadow", &error);
  ASSERT_NE(warren, nullptr) << error;
  ASSERT_TRUE(cottontail::meadowlark::append_text(warren, path, &error))
      << error;

  std::map<std::string, std::string> parameters = {
      {"container", "/."}, {"stemmer", "porter"}};
  EXPECT_FALSE(cottontail::meadowlark::forage(
      warren, path, "(", "tf-idf", "bad-query", parameters, &error, 1));

  std::map<std::string, std::string> bad_stemmer = {
      {"container", "/."}, {"stemmer", "not-a-stemmer"}};
  error.clear();
  EXPECT_FALSE(cottontail::meadowlark::forage(
      warren, path, ":", "tf-idf", "bad-stemmer", bad_stemmer, &error, 1));

  warren->start();
  for (const std::string tag : {"bad-query", "bad-stemmer"}) {
    std::unique_ptr<cottontail::Hopper> definition = warren->hopper_from_gcl(
        "(>> (>> (>> (>> @ (>> :type: \"forager\")) "
        "(>> :name: \"tf-idf\")) (>> :tag: \"" +
            tag + "\")) :query:)",
        &error);
    ASSERT_NE(definition, nullptr) << error;
    cottontail::addr p, q;
    definition->tau(cottontail::minfinity + 1, &p, &q);
    EXPECT_EQ(p, cottontail::maxfinity) << tag;
  }
  warren->end();
}

TEST(Meadowlark, DefaultForagerTagUsesPrimaryDefinition) {
  const std::string path = "test/code.txt";
  std::string error;
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::meadowlark::create_meadow("default-forage.meadow", &error);
  ASSERT_NE(warren, nullptr) << error;
  ASSERT_TRUE(cottontail::meadowlark::append_text(warren, path, &error))
      << error;
  std::map<std::string, std::string> parameters = {
      {"container", "/."}, {"stemmer", "porter"}};
  ASSERT_TRUE(cottontail::meadowlark::forage(
      warren, path, ":", "", "", parameters, &error, 1))
      << error;

  warren->start();
  bool foraged = false;
  ASSERT_TRUE(cottontail::meadowlark::already_foraged(
      warren, path, "", "", &foraged, &error))
      << error;
  EXPECT_TRUE(foraged);
  EXPECT_NE(cottontail::meadowlark::TfIdfForager::make(warren, "", &error),
            nullptr)
      << error;

  std::shared_ptr<cottontail::Stats> stats =
      cottontail::meadowlark::TfIdfStats::make("", warren, &error);
  ASSERT_NE(stats, nullptr) << error;
  EXPECT_EQ(stats->recipe(), "none");
  std::unique_ptr<cottontail::Hopper> actual = stats->container_hopper();
  std::unique_ptr<cottontail::Hopper> expected =
      warren->hopper_from_gcl("/.", &error);
  ASSERT_NE(actual, nullptr);
  ASSERT_NE(expected, nullptr) << error;
  cottontail::addr actual_p, actual_q, expected_p, expected_q;
  actual->tau(cottontail::minfinity + 1, &actual_p, &actual_q);
  expected->tau(cottontail::minfinity + 1, &expected_p, &expected_q);
  EXPECT_EQ(actual_p, expected_p);
  EXPECT_EQ(actual_q, expected_q);
  warren->end();
}

TEST(Meadowlark, TfIdfForageIsScopedToSelectedTsvFile) {
  const std::string tsv = "test/test.tsv";
  const std::string other = "test/sonnet0.txt";
  const std::string tag = "table-scope";
  std::string error;
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::meadowlark::create_meadow("scoped-forage.meadow", &error);
  ASSERT_NE(warren, nullptr) << error;
  ASSERT_TRUE(cottontail::meadowlark::append_tsv(warren, tsv, &error, false,
                                                 "\t", 3))
      << error;
  ASSERT_TRUE(cottontail::meadowlark::append_text(warren, other, &error))
      << error;

  std::map<std::string, std::string> parameters = {
      {"container", ":"}, {"id", ":0:"}, {"stemmer", "porter"}};
  ASSERT_TRUE(cottontail::meadowlark::forage(
      warren, tsv, ":1:", "tf-idf", tag, parameters, &error, 3))
      << error;

  warren->start();
  bool foraged = false;
  ASSERT_TRUE(cottontail::meadowlark::already_foraged(
      warren, tsv, "tf-idf", tag, &foraged, &error))
      << error;
  EXPECT_TRUE(foraged);
  ASSERT_TRUE(cottontail::meadowlark::already_foraged(
      warren, other, "tf-idf", tag, &foraged, &error))
      << error;
  EXPECT_FALSE(foraged);

  std::unique_ptr<cottontail::Hopper> contents =
      warren->hopper_from_gcl("(<< :1: test/test.tsv)", &error);
  ASSERT_NE(contents, nullptr) << error;
  cottontail::addr p, q;
  cottontail::addr expected_items = 0;
  for (contents->tau(cottontail::minfinity + 1, &p, &q);
       p < cottontail::maxfinity; contents->tau(p + 1, &p, &q))
    expected_items++;
  ASSERT_GT(expected_items, 0);

  std::shared_ptr<cottontail::Featurizer> total =
      cottontail::TaggingFeaturizer::make(
          warren->featurizer(), "tf-idf:table-scope:total", &error);
  ASSERT_NE(total, nullptr) << error;
  std::unique_ptr<cottontail::Hopper> totals =
      warren->idx()->hopper(total->featurize("items"));
  ASSERT_NE(totals, nullptr);
  std::unique_ptr<cottontail::Hopper> file =
      warren->idx()->hopper(warren->featurizer()->featurize(tsv));
  ASSERT_NE(file, nullptr);
  cottontail::addr actual_items = 0;
  cottontail::addr n;
  for (totals->tau(cottontail::minfinity + 1, &p, &q, &n);
       p < cottontail::maxfinity; totals->tau(p + 1, &p, &q, &n)) {
    actual_items += n;
    cottontail::addr file_p, file_q;
    file->rho(p, &file_p, &file_q);
    EXPECT_LE(file_p, p);
    EXPECT_GE(file_q, q);
  }
  EXPECT_EQ(actual_items, expected_items);

  std::shared_ptr<cottontail::Featurizer> tf =
      cottontail::TaggingFeaturizer::make(
          warren->featurizer(), "tf-idf:table-scope:tf", &error);
  ASSERT_NE(tf, nullptr) << error;
  std::shared_ptr<cottontail::Stemmer> stemmer =
      cottontail::Stemmer::make("porter", "", &error);
  ASSERT_NE(stemmer, nullptr) << error;
  EXPECT_GT(warren->idx()->count(tf->featurize(stemmer->stem("brown"))), 0);
  EXPECT_EQ(warren->idx()->count(tf->featurize(stemmer->stem("love"))), 0);
  warren->end();
}

TEST(Meadowlark, LegacyTfIdfRemainsReadableButCannotBeExtended) {
  const std::string path = "test/code.txt";
  std::string error;
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::meadowlark::create_meadow("legacy-stats.meadow", &error);
  ASSERT_NE(warren, nullptr) << error;
  ASSERT_TRUE(cottontail::meadowlark::append_text(warren, path, &error))
      << error;

  warren->start();
  ASSERT_TRUE(warren->transaction(&error)) << error;
  const std::string metadata =
      "{\"name\":\"tf-idf\",\"tag\":\"\",\"parameters\":{"
      "\"container\":\":\",\"gcl\":\":\",\"stemmer\":\"porter\"}}";
  cottontail::addr p, q;
  ASSERT_TRUE(warren->appender()->append(metadata, &p, &q, &error)) << error;
  ASSERT_TRUE(warren->annotator()->annotate(
      warren->featurizer()->featurize("@"), p, q, &error))
      << error;
  ASSERT_TRUE(warren->annotator()->annotate(
      warren->featurizer()->featurize("@tf-idf:"), p, q, &error))
      << error;
  std::shared_ptr<cottontail::Featurizer> total =
      cottontail::TaggingFeaturizer::make(warren->featurizer(),
                                          "tf-idf:total", &error);
  ASSERT_NE(total, nullptr) << error;
  ASSERT_TRUE(warren->annotator()->annotate(total->featurize("items"), p, p,
                                            cottontail::addr{1}, &error))
      << error;
  ASSERT_TRUE(warren->annotator()->annotate(total->featurize("length"), p, p,
                                            cottontail::addr{4}, &error))
      << error;
  ASSERT_TRUE(warren->ready(&error)) << error;
  warren->commit();
  warren->end();

  warren->start();
  std::shared_ptr<cottontail::Stats> stats =
      cottontail::meadowlark::TfIdfStats::make("", warren, &error);
  ASSERT_NE(stats, nullptr) << error;
  EXPECT_EQ(stats->recipe(), "");
  EXPECT_DOUBLE_EQ(stats->avgl(), 4.0);
  warren->end();

  std::map<std::string, std::string> parameters = {
      {"container", ":"}, {"stemmer", "porter"}};
  error.clear();
  EXPECT_FALSE(cottontail::meadowlark::forage(
      warren, path, ":", "tf-idf", "", parameters, &error, 1));
  EXPECT_NE(error.find("remains readable but cannot be extended"),
            std::string::npos);
}
