#include "src/working.h"

#include <array>
#include <cassert>
#include <cstring>
#include <fstream>
#include <memory>
#include <regex>
#include <vector>

#include "src/compressor.h"
#include "src/core.h"

namespace cottontail {

namespace {
size_t COMPRESSED_TESTING = 133;
size_t COMPRESSED_AVAILABLE = 64 * 1024 * 1024;
} // namespace

class FileReader final : public Reader {
public:
  static std::shared_ptr<Reader> make(const std::string &name,
                                      std::string *error = nullptr) {
    std::shared_ptr<FileReader> reader =
        std::shared_ptr<FileReader>(new FileReader());
    reader->contents_.open(name, std::ios::binary | std::ios::in);
    if (reader->contents_.fail()) {
      safe_set(error) = "Reader can't access: " + name;
      return nullptr;
    } else {
      return reader;
    }
  };

  virtual ~FileReader(){};
  FileReader(const FileReader &) = delete;
  FileReader &operator=(const FileReader &) = delete;
  FileReader(FileReader &&) = delete;
  FileReader &operator=(FileReader &&) = delete;

private:
  FileReader(){};
  size_t read_(char *buffer, size_t where, size_t amount) final {
    where_ = where;
    return read(buffer, amount);
  };
  size_t read_(char *buffer, size_t amount) final {
    size_t available = size();
    if (amount == 0 || where_ >= available)
      return 0;
    size_t actual;
    if (where_ + amount > available)
      actual = available - where_;
    else
      actual = amount;
    std::streamoff w = where_;
    contents_.seekg(w, contents_.beg);
    assert(!contents_.fail());
    contents_.read(buffer, actual);
    assert(!contents_.fail());
    where_ += actual;
    return actual;
  };
  size_t size_() final {
    if (!cached_size_valid_) {
      contents_.seekg(0, contents_.end);
      assert(!contents_.fail());
      cached_size_ = contents_.tellg();
      cached_size_valid_ = true;
    }
    return cached_size_;
  };
  std::fstream contents_;
  size_t where_ = 0;
  size_t cached_size_ = 0;
  bool cached_size_valid_ = false;
};

class FullReader final : public Reader {
public:
  static std::shared_ptr<Reader> make(const std::string &name,
                                      std::string *error = nullptr) {
    std::shared_ptr<Reader> filereader = Reader::make(name, false, error);
    if (filereader == nullptr)
      return nullptr;
    std::shared_ptr<FullReader> fullreader =
        std::shared_ptr<FullReader>(new FullReader());
    fullreader->buffer_size_ = filereader->size();
    if (fullreader->buffer_size_ == 0)
      return fullreader;
    fullreader->buffer_ =
        std::unique_ptr<char[]>(new char[fullreader->buffer_size_ + 1]);
    filereader->read(fullreader->buffer_.get(), 0, fullreader->buffer_size_);
    fullreader->buffer_[fullreader->buffer_size_] = '\0';
    return fullreader;
  }

  virtual ~FullReader(){};
  FullReader(const FullReader &) = delete;
  FullReader &operator=(const FullReader &) = delete;
  FullReader(FullReader &&) = delete;
  FullReader &operator=(FullReader &&) = delete;

private:
  FullReader(){};
  size_t read_(char *buffer, size_t where, size_t amount) final {
    where_ = where;
    return read(buffer, amount);
  }
  size_t read_(char *buffer, size_t amount) final {
    if (amount == 0 || where_ >= buffer_size_)
      return 0;
    if (where_ + amount > buffer_size_)
      amount = buffer_size_ - where_;
    mempcpy(buffer, buffer_.get() + where_, amount);
    where_ += amount;
    return amount;
  }
  size_t size_() { return buffer_size_; };
  size_t where_ = 0;
  size_t buffer_size_;
  std::unique_ptr<char[]> buffer_;
};

class CompressReader final : public Reader {
public:
  static std::shared_ptr<Reader> make(const std::string &name,
                                      std::string *error = nullptr) {
    std::shared_ptr<CompressReader> reader =
        std::shared_ptr<CompressReader>(new CompressReader());
    reader->current_ = reader->end_ = 0;
    reader->buffer_available_ = COMPRESSED_AVAILABLE;
    reader->buffer_ =
        std::unique_ptr<char[]>(new char[reader->buffer_available_ + 1]);
    std::string compressor_name = "zlib";
    std::string compressor_recipe = "";
    reader->compressor_ =
        Compressor::make(compressor_name, compressor_recipe, error);
    if (reader->compressor_ == nullptr)
      return nullptr;
    reader->compressed_available_ =
        reader->buffer_available_ +
        reader->compressor_->extra(reader->buffer_available_);
    reader->compressed_ =
        std::unique_ptr<char[]>(new char[reader->compressed_available_ + 1]);
    std::string reader_recipe = "file";
    reader->reader_ = Reader::make(name, reader_recipe, error);
    if (reader->reader_ == nullptr)
      return nullptr;
    return reader;
  };

  virtual ~CompressReader(){};
  CompressReader(const CompressReader &) = delete;
  CompressReader &operator=(const CompressReader &) = delete;
  CompressReader(CompressReader &&) = delete;
  CompressReader &operator=(CompressReader &&) = delete;

private:
  CompressReader(){};
  size_t read_(char *buffer, size_t where, size_t amount) {
    assert(false);
    return 0;
  }
  size_t read_(char *buffer, size_t amount) {
    size_t total = 0;
    while (amount > 0 && buffer_available_ > 0) {
      if (end_ - current_ > 0) {
        size_t n = (end_ - current_ >= amount ? amount : end_ - current_);
        memcpy(buffer, buffer_.get() + current_, n);
        buffer += n;
        amount -= n;
        total += n;
        current_ += n;
      } else {
        size_t compressed_size;
        size_t n = reader_->read(reinterpret_cast<char *>(&compressed_size),
                                 sizeof(compressed_size));
        if (n == 0) {
          buffer_available_ = 0;
          return total;
        }
        assert(n == sizeof(compressed_size));
        size_t m = reader_->read(compressed_.get(), compressed_size);
        end_ = compressor_->tang(compressed_.get(), m, buffer_.get(),
                                 buffer_available_);
        current_ = 0;
      }
    }
    return total;
  };
  size_t size_() {
    assert(false);
    return 0;
  };
  size_t end_;
  size_t current_;
  size_t buffer_available_;
  std::unique_ptr<char[]> buffer_;
  std::shared_ptr<Compressor> compressor_;
  size_t compressed_available_;
  std::unique_ptr<char[]> compressed_;
  std::shared_ptr<Reader> reader_;
};

std::shared_ptr<Reader> Reader::make(const std::string &name,
                                     const std::string &recipe,
                                     std::string *error) {
  if (recipe == "" || recipe == "file") {
    return FileReader::make(name, error);
  } else if (recipe == "full") {
    return FullReader::make(name, error);
  } else if (recipe == "compressed" || "compression test") {
    return CompressReader::make(name, error);
  } else {
    safe_set(error) = "Can't make reader with recipe: " + recipe;
    return nullptr;
  }
};

class FileWriter final : public Writer {
public:
  static std::shared_ptr<Writer> make(const std::string &name,
                                      std::string *error = nullptr) {
    std::shared_ptr<FileWriter> writer =
        std::shared_ptr<FileWriter>(new FileWriter());
    writer->contents_.open(name, std::ios::binary | std::ios::out);
    if (writer->contents_.fail()) {
      safe_set(error) = "Writer can't access: " + name;
      return nullptr;
    } else {
      return writer;
    }
  };

  virtual ~FileWriter(){};
  FileWriter(const FileWriter &) = delete;
  FileWriter &operator=(const FileWriter &) = delete;
  FileWriter(FileWriter &&) = delete;
  FileWriter &operator=(FileWriter &&) = delete;

private:
  FileWriter(){};
  void append_(const char *buffer, size_t amount) final {
    assert(!contents_.fail());
    contents_.write(buffer, amount);
    assert(!contents_.fail());
  };
  std::fstream contents_;
};

class CompressWriter final : public Writer {
public:
  static std::shared_ptr<Writer> make(const std::string &name,
                                      std::string *error = nullptr,
                                      bool testing = false) {
    std::shared_ptr<CompressWriter> writer =
        std::shared_ptr<CompressWriter>(new CompressWriter());
    if (testing)
      writer->buffer_available_ = COMPRESSED_TESTING;
    else
      writer->buffer_available_ = COMPRESSED_AVAILABLE;
    writer->buffer_used_ = 0;
    writer->buffer_ =
        std::unique_ptr<char[]>(new char[writer->buffer_available_ + 1]);
    std::string compressor_name = "zlib";
    std::string compressor_recipe = "";
    writer->compressor_ =
        Compressor::make(compressor_name, compressor_recipe, error);
    if (writer->compressor_ == nullptr)
      return nullptr;
    writer->compressed_available_ =
        writer->buffer_available_ +
        writer->compressor_->extra(writer->buffer_available_);
    writer->compressed_ =
        std::unique_ptr<char[]>(new char[writer->compressed_available_ + 1]);
    std::string writer_recipe = "file";
    writer->writer_ = Writer::make(name, writer_recipe, error);
    if (writer->writer_ == nullptr)
      return nullptr;
    return writer;
  };

  virtual ~CompressWriter() { flush(); };
  CompressWriter(const CompressWriter &) = delete;
  CompressWriter &operator=(const CompressWriter &) = delete;
  CompressWriter(CompressWriter &&) = delete;
  CompressWriter &operator=(CompressWriter &&) = delete;

private:
  CompressWriter(){};
  void flush() {
    if (buffer_used_ == 0)
      return;
    size_t compressed_size = compressor_->crush(
        buffer_.get(), buffer_used_, compressed_.get(), compressed_available_);
    writer_->append(reinterpret_cast<char *>(&compressed_size),
                    sizeof(compressed_size));
    writer_->append(compressed_.get(), compressed_size);
    buffer_used_ = 0;
  };
  void append_(const char *buffer, size_t amount) final {
    while (buffer_used_ + amount > buffer_available_) {
      size_t remaining = buffer_available_ - buffer_used_;
      memcpy(buffer_.get() + buffer_used_, buffer, remaining);
      buffer_used_ += remaining;
      flush();
      buffer += remaining;
      amount -= remaining;
    }
    if (amount == 0)
      return;
    memcpy(buffer_.get() + buffer_used_, buffer, amount);
    buffer_used_ += amount;
  };
  size_t buffer_used_;
  size_t buffer_available_;
  std::unique_ptr<char[]> buffer_;
  std::shared_ptr<Compressor> compressor_;
  size_t compressed_available_;
  std::unique_ptr<char[]> compressed_;
  std::shared_ptr<Writer> writer_;
};

std::shared_ptr<Writer> Writer::make(const std::string &name,
                                     const std::string &recipe,
                                     std::string *error) {
  if (recipe == "" || recipe == "file" || recipe == "full") {
    return FileWriter::make(name, error);
  } else if (recipe == "compressed") {
    return CompressWriter::make(name, error);
  } else if (recipe == "compression test") {
    return CompressWriter::make(name, error, true);
  } else {
    safe_set(error) = "Can't make writer with recipe: " + recipe;
    return nullptr;
  }
};

std::vector<std::string> Working::ls(const std::string &prefix) {
  std::string ls_command;
  FILE *fp = fopen("/usr/bin/ls", "r");
  if (fp) {
    fclose(fp);
    ls_command = "/usr/bin/ls " + working_;
  } else {
    fp = fopen("/bin/ls", "r");
    assert(fp);
    fclose(fp);
    ls_command = "/bin/ls " + working_;
  }
  FILE *pipe = popen(ls_command.c_str(), "r");
  assert(pipe);
  std::string pattern = "^" + prefix + "\\.*";
  std::regex r(pattern);
  std::vector<std::string> names;
  const size_t buffer_size = 256;
  std::array<char, buffer_size> buffer;
  while (fgets(buffer.data(), buffer_size, pipe) != NULL) {
    if (!std::regex_search(buffer.data(), r))
      continue;
    for (char *s = buffer.data(); *s != '\0'; s++)
      if (*s == '\n') {
        *s = '\0';
        break;
      }
    names.emplace_back(buffer.data());
  }
  pclose(pipe);
  return names;
}

} // namespace cottontail
