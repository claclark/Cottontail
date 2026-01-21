#include "gtest/gtest.h"

#include <string>

#include "src/cottontail.h"

TEST(Working, File) {
  std::string error;
  std::string burrow_name = "the.burrow";
  std::shared_ptr<cottontail::Working> working =
      cottontail::Working::mkdir(burrow_name, &error);
  ASSERT_NE(working, nullptr);
  std::string name = "the.file";
  size_t LOTS = 1024 * 1024;
  {
    std::string recipe = "file";
    std::shared_ptr<cottontail::Writer> writer = working->writer(name, recipe);
    ASSERT_NE(writer, nullptr);
    for (size_t i = 0; i < LOTS; i++)
      writer->append(reinterpret_cast<char *>(&i), sizeof(i));
  }
  {
    std::string recipe = "file";
    std::shared_ptr<cottontail::Reader> reader = working->reader(name, recipe);
    ASSERT_NE(reader, nullptr);
    for (size_t i = 0; i < LOTS; i++) {
      size_t j;
      size_t k =
          reader->read(reinterpret_cast<char *>(&j), i * sizeof(j), sizeof(j));
      ASSERT_EQ(i, j);
      ASSERT_EQ(k, sizeof(j));
    }
    {
      size_t j;
      size_t k = reader->read(reinterpret_cast<char *>(&j), LOTS * sizeof(j),
                              sizeof(j));
      ASSERT_EQ(k, (size_t)0);
    }
    for (size_t i = LOTS; i > 0; --i) {
      size_t j;
      size_t k = reader->read(reinterpret_cast<char *>(&j), (i - 1) * sizeof(j),
                              sizeof(j));
      ASSERT_EQ(i - 1, j);
      ASSERT_EQ(k, sizeof(j));
    }
  }
  {
    std::string recipe = "full";
    std::shared_ptr<cottontail::Reader> reader = working->reader(name, recipe);
    ASSERT_NE(reader, nullptr);
    for (size_t i = 0; i < LOTS; i++) {
      size_t j;
      size_t k =
          reader->read(reinterpret_cast<char *>(&j), i * sizeof(j), sizeof(j));
      ASSERT_EQ(i, j);
      ASSERT_EQ(k, sizeof(j));
    }
    {
      size_t j;
      size_t k = reader->read(reinterpret_cast<char *>(&j), LOTS * sizeof(j),
                              sizeof(j));
      ASSERT_EQ(k, (size_t)0);
    }
    for (size_t i = LOTS; i > 0; --i) {
      size_t j;
      size_t k = reader->read(reinterpret_cast<char *>(&j), (i - 1) * sizeof(j),
                              sizeof(j));
      ASSERT_EQ(i - 1, j);
      ASSERT_EQ(k, sizeof(j));
    }
  }
}

namespace {
std::string junk(size_t i) {
  if (i % 3 == 0)
    return "foo " + std::to_string(i) + " bar ";
  else if (i % 3 == 1)
    return "hello " + std::to_string(i) + " world ";
  else
    return "The Cat in the Hat " + std::to_string(i) + " Came Back ";
}

} // namespace

TEST(Working, Compress) {
  std::string error;
  std::string burrow_name = "the.burrow";
  std::shared_ptr<cottontail::Working> working =
      cottontail::Working::mkdir(burrow_name, &error);
  ASSERT_NE(working, nullptr);
  std::string name = "the.compressed.file";
  std::string recipe = "compression test";
  {
    std::shared_ptr<cottontail::Writer> writer = working->writer(name, recipe);
    for (size_t i = 0; i < 997; i++) {
      std::string thing = junk(i);
      writer->append(thing.c_str(), thing.length());
    }
  }
  {
    char buffer[1024];
    std::shared_ptr<cottontail::Reader> reader = working->reader(name, recipe);
    for (size_t i = 0; i < 997; i++) {
      std::string thing = junk(i);
      size_t n = reader->read(buffer, thing.length());
      ASSERT_EQ(n, thing.length());
      buffer[n] = '\0';
      std::string think(buffer);
      ASSERT_EQ(think, thing);
    }
  }
}
