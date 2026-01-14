#include <memory>
#include <string>

#include "gtest/gtest.h"

#include "meadowlark/meadowlark.h"
#include "src/cottontail.h"

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
