#include <atomic>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

#include <zlib.h>

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

TEST(ZlibCompressor, ReusePreservesZlibFormat) {
  std::shared_ptr<cottontail::Compressor> compressor =
      cottontail::ZlibCompressor::make();
  std::vector<std::string> samples = {
      "a",
      "the cat in the hat",
      std::string("\0\1\2\3\4\5\6\7", 8),
      std::string(4096, 'x'),
  };
  for (size_t repeat = 0; repeat < 3; repeat++)
    for (auto &sample : samples) {
      size_t available = sample.size() + compressor->extra(sample.size());
      EXPECT_EQ(available, compressBound(sample.size()));
      std::vector<char> actual(available);
      size_t actual_size = compressor->crush(
          sample.data(), sample.size(), actual.data(), actual.size());

      uLongf expected_size = compressBound(sample.size());
      std::vector<Bytef> expected(expected_size);
      ASSERT_EQ(compress2(expected.data(), &expected_size,
                          reinterpret_cast<const Bytef *>(sample.data()),
                          sample.size(), Z_BEST_COMPRESSION),
                Z_OK);
      ASSERT_EQ(actual_size, expected_size);
      EXPECT_EQ(memcmp(actual.data(), expected.data(), actual_size), 0);

      std::vector<char> restored(sample.size());
      size_t restored_size =
          compressor->tang(actual.data(), actual_size, restored.data(),
                           restored.size());
      ASSERT_EQ(restored_size, sample.size());
      EXPECT_EQ(memcmp(restored.data(), sample.data(), sample.size()), 0);
    }
}

TEST(ZlibCompressor, SharedCompressorIsThreadSafe) {
  std::shared_ptr<cottontail::Compressor> compressor =
      cottontail::ZlibCompressor::make();
  std::atomic<bool> okay = true;
  std::vector<std::thread> workers;
  for (size_t worker = 0; worker < 8; worker++)
    workers.emplace_back([compressor, worker, &okay]() {
      for (size_t iteration = 0; iteration < 100; iteration++) {
        std::string sample =
            "worker " + std::to_string(worker) + " iteration " +
            std::to_string(iteration) + " " +
            std::string(32 + worker + iteration,
                        static_cast<char>('a' + worker));
        size_t available = sample.size() + compressor->extra(sample.size());
        std::vector<char> crushed(available);
        size_t crushed_size =
            compressor->crush(sample.data(), sample.size(), crushed.data(),
                              crushed.size());
        std::vector<char> restored(sample.size());
        size_t restored_size =
            compressor->tang(crushed.data(), crushed_size, restored.data(),
                             restored.size());
        if (restored_size != sample.size() ||
            memcmp(restored.data(), sample.data(), sample.size()) != 0)
          okay = false;
      }
    });
  for (auto &worker : workers)
    worker.join();
  EXPECT_TRUE(okay);
}
