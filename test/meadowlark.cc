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
  std::string burrow = "tsv.meadow";
  std::string options = "tokenizer:name:utf8 txt:json:yes featurizer@json";
  std::shared_ptr<cottontail::Bigwig> warren =
      cottontail::Bigwig::make(burrow, options);
  ASSERT_NE(warren, nullptr);
  ASSERT_TRUE(cottontail::meadowlark::append_tsv(warren, "test/test.tsv",
                                                 nullptr, true));
  warren->start();
  std::shared_ptr<cottontail::Hopper> hopper =
      warren->hopper_from_gcl("(<< :0: (>> : (>> :3: \"Mud bath\")))");
  ASSERT_NE(hopper, nullptr);
  cottontail::addr p, q;
  hopper->rho(0, &p, &q);
  std::string pig = warren->txt()->translate(p, q).substr(0, 3);
  EXPECT_EQ(pig, "Pig");
  hopper = warren->hopper_from_gcl("(<< :2: (>> : (>> :0: \"Owl\")))");
  ASSERT_NE(hopper, nullptr);
  hopper->ohr(10000, &p, &q);
  std::string mouse = warren->txt()->translate(p, q).substr(0, 5);
  EXPECT_EQ(mouse, "Mouse");
  warren->end();
}
