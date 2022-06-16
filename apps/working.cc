#define ASSERT_EQ(a, b) (assert((a) == (b)))
#define ASSERT_NE(a, b) (assert((a) != (b)))
#define ASSERT_TRUE(a) (assert((a)))
#define ASSERT_FALSE(a) (assert(!(a)))
#define EXPECT_EQ(a, b) (assert((a) == (b)))
#define EXPECT_NE(a, b) (assert((a) != (b)))
#define EXPECT_TRUE(a) (assert((a)))
#define EXPECT_FALSE(a) (assert(!(a)))

#include <cstring>
#define EXPECT_STREQ(a, b) (assert(strcmp((a), (b)) == 0))

#include <memory>
#include <string>

#include "src/compressor.h"
#include "src/simple_txt_io.h"

void test_update(std::string compressor_name, std::streamsize chunk_size) {
  std::string error;
  std::shared_ptr<cottontail::Compressor> compressor =
      cottontail::Compressor::make("zlib", "", &error);
  EXPECT_NE(compressor, nullptr);
  std::string nameof_contents = "update-" + compressor_name + "-" +
                                std::to_string(chunk_size) + ".contents";
  std::string nameof_chunk_map =
      "update-" + compressor_name + "-" + std::to_string(chunk_size) + ".map";
  char test0[] =
      "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do "
      "eiusmod tempor incididunt ut labore et dolore magna aliqua. ";
  char test1[] = "Ut enim ad minim veniam, quis nostrud exercitation ullamco "
                 "laboris nisi ut aliquip ex ea commodo consequat. ";
  char test2[] =
      "Mauris pellentesque pulvinar pellentesque habitant morbi. A diam "
      "maecenas sed enim ut. Dignissim diam quis enim lobortis. ";
  cottontail::addr expected_size = 0;
  for (size_t i = 0; i < 100; i++) {
    {
      std::shared_ptr<cottontail::SimpleTxtIO> io =
          cottontail::SimpleTxtIO::make(nameof_contents, nameof_chunk_map,
                                        chunk_size, compressor, &error);
      io->append(test0, sizeof(test0) - 1);
      expected_size += (sizeof(test0) - 1);
      EXPECT_EQ(expected_size, io->size());
    }
    {
      std::shared_ptr<cottontail::SimpleTxtIO> io =
          cottontail::SimpleTxtIO::make(nameof_contents, nameof_chunk_map,
                                        chunk_size, compressor, &error);
      io->append(test1, sizeof(test1) - 1);
      expected_size += (sizeof(test1) - 1);
      EXPECT_EQ(expected_size, io->size());
    }
    {
      std::shared_ptr<cottontail::SimpleTxtIO> io =
          cottontail::SimpleTxtIO::make(nameof_contents, nameof_chunk_map,
                                        chunk_size, compressor, &error);
      io->append(test2, sizeof(test2) - 1);
      expected_size += (sizeof(test2) - 1);
      EXPECT_EQ(expected_size, io->size());
    }
  }
  std::shared_ptr<cottontail::SimpleTxtIO> io = cottontail::SimpleTxtIO::make(
      nameof_contents, nameof_chunk_map, chunk_size, compressor, &error);
  cottontail::addr n;
  size_t a = (sizeof(test0) - 1) + (sizeof(test1) - 1) + (sizeof(test2) - 1);
  size_t b = sizeof(test0) - 9;
  for (size_t i = 0; i < 99; i++) {
    std::unique_ptr<char[]> x = io->read(i * a + b, 15, &n);
    EXPECT_EQ(n, 15);
    EXPECT_STREQ(x.get(), "aliqua. Ut enim");
  }
}

int main(int argc, char **argv) {
  test_update("zlib", 3);
  test_update("zlib", 7);
  test_update("zlib", 16);
  test_update("zlib", 30);
  test_update("zlib", 42);
  test_update("zlib", 1024);

  return 1;
}
