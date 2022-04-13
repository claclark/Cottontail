#include <memory>
#include <string>

#include "gtest/gtest.h"

#include "src/zlib_compressor.h"

namespace {

void compression_test(std::shared_ptr<cottontail::Compressor> compressor,
                      std::string s, size_t expected_compressed_size) {
  char prefix[32];
  strncpy(prefix, s.c_str(), 31);
  prefix[31] = '\0';
  std::string sample = prefix;
  if (s.size() > 31)
    sample += "...";
  std::string explanation = "While compressing: \"" + sample + "\"";
  char target[s.size() + 1];
  strcpy(target, s.c_str());
  size_t available = s.size() + compressor->extra(s.size()) + 1;
  char crushed[available];
  size_t actual_compressed_size =
      compressor->crush(target, s.size(), crushed, available);
  EXPECT_EQ(actual_compressed_size, expected_compressed_size) << explanation;
  char tanged[s.size() + 1];
  size_t actual_uncompressed_size =
      compressor->tang(crushed, actual_compressed_size, tanged, s.size() + 1);
  tanged[actual_uncompressed_size] = '\0';
  EXPECT_EQ(actual_uncompressed_size, s.size()) << explanation;
  EXPECT_STREQ(target, tanged) << explanation;
}
} // namespace

TEST(ZlibCompressor, Basic) {
  std::string good_recipe = "";
  EXPECT_TRUE(cottontail::ZlibCompressor::check(good_recipe));
  std::string bad_recipe = "bad recipe";
  EXPECT_FALSE(cottontail::ZlibCompressor::check(bad_recipe));
  std::shared_ptr<cottontail::Compressor> compressor =
      cottontail::ZlibCompressor::make();
  compression_test(
      compressor,
      "Now is the time for all good men to come to the aid of the party.", 65);
  compression_test(compressor,
                   "the cat in the hat the cat in the hat the cat in the hat",
                   27);
  compression_test(
      compressor,
      "hooray hooray hooray hooray hooray hooray hooray hooray hooray hooray "
      "hooray hooray hooray hooray hooray hooray hooray hooray hooray hooray "
      "hooray hooray hooray hooray hooray hooray hooray hooray hooray hooray "
      "hooray hooray hooray hooray hooray hooray hooray hooray hooray hooray "
      "hooray hooray hooray hooray hooray hooray hooray hooray hooray hooray "
      "hooray hooray hooray hooray hooray hooray hooray hooray hooray hooray "
      "hooray hooray hooray hooray hooray hooray hooray hooray hooray hooray "
      "hooray hooray hooray hooray hooray hooray hooray hooray hooray hooray "
      "hooray hooray hooray hooray hooray hooray hooray hooray hooray hooray "
      "hooray hooray hooray hooray hooray hooray hooray hooray hooray hooray "
      "hello world hello world the cat in the hat THE END",
      55);
}
