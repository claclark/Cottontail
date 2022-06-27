#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>

#include "gtest/gtest.h"

#include "src/compressor.h"
#include "src/simple_txt_io.h"

namespace {

void test_with_compressor(std::string compressor_name,
                          std::streamsize chunk_size) {
  std::string explanation = "Using compressor \"" + compressor_name +
                            "\" with chunk_size " + std::to_string(chunk_size);
  std::string empty_recipe = "";
  std::string error;
  std::shared_ptr<cottontail::Compressor> compressor =
      cottontail::Compressor::make(compressor_name, empty_recipe, &error);
  EXPECT_NE(compressor, nullptr);
  std::string nameof_contents =
      compressor_name + "-" + std::to_string(chunk_size) + ".contents";
  std::string nameof_chunk_map =
      compressor_name + "-" + std::to_string(chunk_size) + ".map";
  cottontail::addr expected_size;
  {
    std::shared_ptr<cottontail::SimpleTxtIO> io = cottontail::SimpleTxtIO::make(
        nameof_contents, nameof_chunk_map, chunk_size, compressor, &error);
    char a[] = "Hello world hello world...";
    io->append(a, sizeof(a) - 1);
    expected_size = (sizeof(a) - 1);
    EXPECT_EQ(expected_size, io->size());
    char b[] = "The cat in the hat...";
    io->append(b, sizeof(b) - 1);
    expected_size += (sizeof(b) - 1);
    EXPECT_EQ(expected_size, io->size());
    char c[] = "Testing one two three...";
    io->append(c, sizeof(c) - 1);
    expected_size += (sizeof(c) - 1);
    EXPECT_EQ(expected_size, io->size());
  }
  {
    std::shared_ptr<cottontail::SimpleTxtIO> io = cottontail::SimpleTxtIO::make(
        nameof_contents, nameof_chunk_map, chunk_size, compressor, &error);
    EXPECT_EQ(expected_size, io->size());
    cottontail::addr n;
    std::unique_ptr<char[]> w = io->read(11, 7, &n);
    EXPECT_EQ(n, 7) << explanation;
    EXPECT_STREQ(w.get(), " hello ") << explanation;
    std::unique_ptr<char[]> x = io->read(30, 3, &n);
    EXPECT_EQ(n, 3) << explanation;
    EXPECT_STREQ(x.get(), "cat") << explanation;
    std::unique_ptr<char[]> y = io->read(30, 100, &n);
    EXPECT_EQ(n, 41) << explanation;
    EXPECT_STREQ(y.get(), "cat in the hat...Testing one two three...")
        << explanation;
    std::unique_ptr<char[]> z = io->read(63, 5, &n);
    EXPECT_EQ(n, 5) << explanation;
    EXPECT_STREQ(z.get(), "three") << explanation;
  }
}
} // namespace

TEST(SimpleTxtIO, Null) {
  test_with_compressor("null", 3);
  test_with_compressor("null", 7);
  test_with_compressor("null", 16);
  test_with_compressor("null", 30);
  test_with_compressor("null", 42);
  test_with_compressor("null", 1024);
}

TEST(SimpleTxtIO, Zlib) {
  test_with_compressor("zlib", 3);
  test_with_compressor("zlib", 7);
  test_with_compressor("zlib", 16);
  test_with_compressor("zlib", 30);
  test_with_compressor("zlib", 42);
  test_with_compressor("zlib", 1024);
}

TEST(SimpleTxtIO, Bad) {
  test_with_compressor("bad", 3);
  test_with_compressor("bad", 7);
  test_with_compressor("bad", 16);
  test_with_compressor("bad", 30);
  test_with_compressor("bad", 42);
  test_with_compressor("bad", 1024);
}

namespace {
void test_update(std::string compressor_name, std::streamsize chunk_size) {
  std::string error;
  std::shared_ptr<cottontail::Compressor> compressor =
      cottontail::Compressor::make(compressor_name, "", &error);
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
  cottontail::addr n;
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
      std::unique_ptr<char[]> x = io->read(10, 20, &n);
      EXPECT_EQ(n, 20);
      EXPECT_STREQ(x.get(), "m dolor sit amet, co");
      std::unique_ptr<char[]> y = io->read(1000000000, 20, &n);
      EXPECT_EQ(n, 0);
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
  size_t a = (sizeof(test0) - 1) + (sizeof(test1) - 1) + (sizeof(test2) - 1);
  size_t b = sizeof(test0) - 9;
  for (size_t i = 0; i < 99; i++) {
    std::unique_ptr<char[]> x = io->read(i * a + b, 15, &n);
    EXPECT_EQ(n, 15);
    EXPECT_STREQ(x.get(), "aliqua. Ut enim");
  }
}

} // namespace

TEST(UpdateTxtIO, Null) {
  test_update("null", 3);
  test_update("null", 7);
  test_update("null", 16);
  test_update("null", 30);
  test_update("null", 42);
  test_update("null", 1024);
}

TEST(UpdateTxtIO, Zlib) {
  test_update("zlib", 3);
  test_update("zlib", 7);
  test_update("zlib", 16);
  test_update("zlib", 30);
  test_update("zlib", 42);
  test_update("zlib", 1024);
}

TEST(UpdateTxtIO, Bad) {
  test_update("bad", 3);
  test_update("bad", 7);
  test_update("bad", 16);
  test_update("bad", 30);
  test_update("bad", 42);
  test_update("bad", 1024);
}

void test_transaction(std::string compressor_name, std::streamsize chunk_size) {
  std::string error;
  std::shared_ptr<cottontail::Compressor> compressor =
      cottontail::Compressor::make(compressor_name, "", &error);
  EXPECT_NE(compressor, nullptr);
  std::string nameof_contents = "trans-" + compressor_name + "-" +
                                std::to_string(chunk_size) + ".contents";
  std::string nameof_chunk_map =
      "trans-" + compressor_name + "-" + std::to_string(chunk_size) + ".map";
  char test0[] =
      "Four score and seven years ago our fathers brought forth on "
      "this continent, a new nation, conceived in Liberty, and "
      "dedicated to the proposition that all men are created equal. ";
  char test1[] =
      "Never gonna give you up; Never gonna let you down; Never gonna "
      "run around and desert you ";
  char test2[] =
      "Now we are engaged in a great civil war, testing whether that nation, "
      "or any nation so conceived and so dedicated, can long endure. ";
  cottontail::addr n;
  cottontail::addr expected_size = 0;
  {
    std::shared_ptr<cottontail::SimpleTxtIO> io = cottontail::SimpleTxtIO::make(
        nameof_contents, nameof_chunk_map, chunk_size, compressor, &error);
    EXPECT_TRUE(io->transaction());
    io->append(test1, sizeof(test1) - 1);
    EXPECT_EQ(sizeof(test1) - 1, io->size());
    std::unique_ptr<char[]> y = io->read(sizeof(test1) - 5, 3, &n);
    EXPECT_EQ(n, 3);
    EXPECT_STREQ(y.get(), "you");
    io->abort();
    EXPECT_EQ(0, io->size());
  }
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
      std::unique_ptr<char[]> x = io->read(expected_size - 7, 5, &n);
      EXPECT_EQ(n, 5);
      EXPECT_STREQ(x.get(), "equal");
      EXPECT_TRUE(io->transaction());
      io->append(test1, sizeof(test1) - 1);
      expected_size += (sizeof(test1) - 1);
      {
        std::shared_ptr<cottontail::SimpleTxtIO> jo =
            cottontail::SimpleTxtIO::make(nameof_contents, nameof_chunk_map,
                                          chunk_size, compressor, &error);
        std::unique_ptr<char[]> xyzzy = jo->read(jo->size() - 7, 5, &n);
        EXPECT_EQ(n, 5);
        EXPECT_STREQ(xyzzy.get(), "equal");
      }
      EXPECT_EQ(expected_size, io->size());
      std::unique_ptr<char[]> y = io->read(expected_size - 4, 3, &n);
      EXPECT_EQ(n, 3);
      EXPECT_STREQ(y.get(), "you");
      io->abort();
      expected_size -= (sizeof(test1) - 1);
      EXPECT_EQ(expected_size, io->size());
      std::unique_ptr<char[]> z = io->read(expected_size - 15, 7, &n);
      EXPECT_EQ(n, 7);
      EXPECT_STREQ(z.get(), "created");
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
  cottontail::addr m = (sizeof(test0) - 1) + (sizeof(test2) - 1);
  char test3[m + 1];
  strcpy(test3, test0);
  strcpy(test3 + (sizeof(test0) - 1), test2);
  for (size_t i = 0; i < 100; i++) {
    std::unique_ptr<char[]> x = io->read(i * m, m, &n);
    EXPECT_EQ(n, m);
    EXPECT_STREQ(x.get(), test3);
  }
}

TEST(TransactionTxtIO, Null) {
  test_transaction("null", 3);
  test_transaction("null", 7);
  test_transaction("null", 16);
  test_transaction("null", 30);
  test_transaction("null", 42);
  test_transaction("null", 1024);
}

TEST(TransactionTxtIO, Zlib) {
  test_transaction("zlib", 3);
  test_transaction("zlib", 7);
  test_transaction("zlib", 16);
  test_transaction("zlib", 30);
  test_transaction("zlib", 42);
  test_transaction("zlib", 1024);
}

TEST(TransactionTxtIO, Bad) {
  test_transaction("bad", 3);
  test_transaction("bad", 7);
  test_transaction("bad", 16);
  test_transaction("bad", 30);
  test_transaction("bad", 42);
  test_transaction("bad", 1024);
}
