#include <memory>
#include <string>

#include "gtest/gtest.h"

#include "src/compressor.h"
#include "src/post_compressor.h"

TEST(PostCompressor, Basic) {
  std::string error;
  std::string recipe = "";
  EXPECT_TRUE(cottontail::PostCompressor::check(recipe, &error));
  recipe = "bad recipe";
  EXPECT_FALSE(cottontail::PostCompressor::check(recipe, &error));
  std::string empty_recipe = "";
  std::string compressor_name = "post";
  std::shared_ptr<cottontail::Compressor> compressor =
      cottontail::Compressor::make(compressor_name, empty_recipe, &error);
  cottontail::addr list[] = {10, 200, 1000, 2000, 999999, 1000000, 10000000000};
  char out[1000];
  size_t n = compressor->crush(reinterpret_cast<char *>(list), sizeof(list),
                               out, 1000);
  EXPECT_EQ(n, (size_t) 16);
  cottontail::addr tsil[1000];
  size_t m = compressor->tang(out, n, reinterpret_cast<char *>(tsil), 1000);
  ASSERT_EQ(m, sizeof(list));
  for (size_t i = 0; i < m/sizeof(cottontail::addr); i++)
    EXPECT_EQ(list[i], tsil[i]);
}
