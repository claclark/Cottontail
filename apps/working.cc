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

void test_transaction(std::string compressor_name, std::streamsize chunk_size) {
  std::string error;
  std::shared_ptr<cottontail::Compressor> compressor =
      cottontail::Compressor::make(compressor_name, "", &error);
  EXPECT_NE(compressor, nullptr);
  std::string nameof_contents = "recover-" + compressor_name + "-" +
                                std::to_string(chunk_size) + ".contents";
  std::string nameof_chunk_map =
      "recover-" + compressor_name + "-" + std::to_string(chunk_size) + ".map";
  char test0[] =
      "Stately, plump Buck Mulligan came from the stairhead, bearing a bowl of "
      "lather on which a mirror and a razor lay crossed. A yellow "
      "dressinggown, ungirdled, was sustained gently behind him on the mild "
      "morning air. He held the bowl aloft and intoned: ";
  char test1[] = "—\emph{Introibo ad altare Dei.} ";
  char test2[] = "Halted, he peered down the dark winding stairs and called "
                 "out coarsely: ";
  cottontail::addr n;
  cottontail::addr expected_size = 0;
  for (size_t i = 0; i < 100; i++) {
    {
      std::shared_ptr<cottontail::SimpleTxtIO> io =
          cottontail::SimpleTxtIO::make(nameof_contents, nameof_chunk_map,
                                        chunk_size, compressor, &error);
      EXPECT_TRUE(io->transaction());
      io->append(test0, sizeof(test0) - 1);
      expected_size += (sizeof(test0) - 1);
      EXPECT_EQ(expected_size, io->size());
      EXPECT_TRUE(io->ready());
      io->commit();
    }
    {
      std::shared_ptr<cottontail::SimpleTxtIO> io =
          cottontail::SimpleTxtIO::make(nameof_contents, nameof_chunk_map,
                                        chunk_size, compressor, &error);
      EXPECT_TRUE(io->transaction());
      io->append(test1, sizeof(test1) - 1);
      expected_size += (sizeof(test1) - 1);
      EXPECT_EQ(expected_size, io->size());
      std::unique_ptr<char[]> x = io->read(expected_size - 6, 3, &n);
      EXPECT_EQ(n, 3);
      EXPECT_STREQ(x.get(), "Dei");
      EXPECT_TRUE(io->ready());
    }
    expected_size -= (sizeof(test1) - 1);
    {
      std::shared_ptr<cottontail::SimpleTxtIO> io =
          cottontail::SimpleTxtIO::make(nameof_contents, nameof_chunk_map,
                                        chunk_size, compressor, &error);
      std::unique_ptr<char[]> x = io->read(expected_size - 9, 7, &n);
      EXPECT_EQ(n, 7);
      EXPECT_STREQ(x.get(), "intoned");
      EXPECT_FALSE(io->transaction());
    }
    cottontail::SimpleTxtIO::recover(nameof_contents, nameof_chunk_map,
                                     chunk_size, compressor, true);
    expected_size += (sizeof(test1) - 1);
    {
      std::shared_ptr<cottontail::SimpleTxtIO> io =
          cottontail::SimpleTxtIO::make(nameof_contents, nameof_chunk_map,
                                        chunk_size, compressor, &error);
      std::unique_ptr<char[]> x = io->read(expected_size - 6, 3, &n);
      EXPECT_EQ(n, 3);
      EXPECT_STREQ(x.get(), "Dei");
    }
    {
      std::shared_ptr<cottontail::SimpleTxtIO> io =
          cottontail::SimpleTxtIO::make(nameof_contents, nameof_chunk_map,
                                        chunk_size, compressor, &error);
      EXPECT_TRUE(io->transaction());
      io->append(test2, sizeof(test2) - 1);
      expected_size += (sizeof(test2) - 1);
      EXPECT_EQ(expected_size, io->size());
      std::unique_ptr<char[]> x = io->read(expected_size - 10, 8, &n);
      EXPECT_EQ(n, 8);
      EXPECT_STREQ(x.get(), "coarsely");
    }
    cottontail::SimpleTxtIO::recover(nameof_contents, nameof_chunk_map,
                                     chunk_size, compressor, false);
    expected_size -= (sizeof(test2) - 1);
  }
  std::shared_ptr<cottontail::SimpleTxtIO> io = cottontail::SimpleTxtIO::make(
      nameof_contents, nameof_chunk_map, chunk_size, compressor, &error);
  cottontail::addr m = (sizeof(test0) - 1) + (sizeof(test1) - 1);
  char test3[m + 1];
  strcpy(test3, test0);
  strcpy(test3 + (sizeof(test0) - 1), test1);
  for (size_t i = 0; i < 100; i++) {
    std::unique_ptr<char[]> x = io->read(i * m, m, &n);
    EXPECT_EQ(n, m);
    EXPECT_STREQ(x.get(), test3);
  }
}

int main(int argc, char **argv) {
  test_transaction("null", 3);
  test_transaction("null", 7);
  test_transaction("null", 16);
  test_transaction("null", 30);
  test_transaction("null", 42);
  test_transaction("null", 1024);

  test_transaction("zlib", 3);
  test_transaction("zlib", 7);
  test_transaction("zlib", 16);
  test_transaction("zlib", 30);
  test_transaction("zlib", 42);
  test_transaction("zlib", 1024);

  test_transaction("bad", 3);
  test_transaction("bad", 7);
  test_transaction("bad", 16);
  test_transaction("bad", 30);
  test_transaction("bad", 42);
  test_transaction("bad", 1024);

  return 1;
}
