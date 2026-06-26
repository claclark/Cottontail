#include "src/builder.h"

#include <array>
#include <cassert>
#include <cstdio>
#include <exception>
#include <iostream>
#include <istream>
#include <memory>
#include <regex>
#include <streambuf>
#include <string>
#include <vector>

#include "src/core.h"

namespace cottontail {
namespace {

inline bool has_extension(const std::string &filename,
                          const std::string &extension) {
  return filename.size() > extension.size() &&
         filename.substr(filename.size() - extension.size()) == extension;
}

inline bool has_zcat_extension(const std::string &filename) {
  return has_extension(filename, ".z") || has_extension(filename, ".Z") ||
         has_extension(filename, ".gz") || has_extension(filename, ".GZ") ||
         // plus strange extensions used on old TREC collections
         has_extension(filename, ".0z") || has_extension(filename, ".1z") ||
         has_extension(filename, ".2z");
}

std::string shell_quote(const std::string &s) {
  std::string quoted = "'";
  for (char c : s) {
    if (c == '\'')
      quoted += "'\\''";
    else
      quoted += c;
  }
  quoted += "'";
  return quoted;
}

bool zcat(std::string *path) {
  FILE *fp = fopen("/usr/bin/zcat", "r");
  if (fp != nullptr) {
    fclose(fp);
    *path = "/usr/bin/zcat";
    return true;
  }
  fp = fopen("/bin/zcat", "r");
  if (fp != nullptr) {
    fclose(fp);
    *path = "/bin/zcat";
    return true;
  }
  return false;
}

class FileStreamBuffer final : public std::streambuf {
public:
  FileStreamBuffer(FILE *file, bool pipe) : file_(file), pipe_(pipe) {}
  ~FileStreamBuffer() final { close(); }

protected:
  int_type underflow() final {
    if (gptr() < egptr())
      return traits_type::to_int_type(*gptr());
    if (file_ == nullptr)
      return traits_type::eof();
    size_t count = fread(buffer_.data(), 1, buffer_.size(), file_);
    if (count == 0)
      return traits_type::eof();
    setg(buffer_.data(), buffer_.data(), buffer_.data() + count);
    return traits_type::to_int_type(*gptr());
  }

private:
  void close() {
    if (file_ == nullptr)
      return;
    if (pipe_)
      pclose(file_);
    else
      fclose(file_);
    file_ = nullptr;
  }

  FILE *file_;
  bool pipe_;
  static constexpr size_t BUFFER_SIZE = 256 * 1024;
  std::array<char, BUFFER_SIZE> buffer_;
};

class FileInputStream final : public std::istream {
public:
  FileInputStream(FILE *file, bool pipe)
      : std::istream(nullptr), buffer_(file, pipe) {
    rdbuf(&buffer_);
  }

private:
  FileStreamBuffer buffer_;
};

} // namespace

std::unique_ptr<std::istream> maybe_zipped(const std::string &filename,
                                           std::string *error) {
  FILE *fp = fopen(filename.c_str(), "r");
  if (!fp) {
    safe_error(error) = "Could not access: " + filename;
    return nullptr;
  }
  if (!has_zcat_extension(filename))
    return std::make_unique<FileInputStream>(fp, false);
  fclose(fp);
  // All this is pretty ugly..
  // ...and won't work at all on things that aren't fairly unix-like.
  // TODO: replace with a call to a library function?
  std::string zcat_path;
  if (!zcat(&zcat_path)) {
    safe_error(error) = "Can't find zcat in any of the usual places";
    return nullptr;
  }
  std::string zcat_command = zcat_path + " " + shell_quote(filename);
  FILE *pipe = popen(zcat_command.c_str(), "r");
  if (!pipe) {
    safe_error(error) = "Can't run zcat command: " + zcat_command;
    return nullptr;
  }
  return std::make_unique<FileInputStream>(pipe, true);
}

std::shared_ptr<std::string> inhale(const std::string &filename,
                                    std::string *error) {
  std::unique_ptr<std::istream> input = maybe_zipped(filename, error);
  if (input == nullptr)
    return nullptr;
  std::shared_ptr<std::string> contents = std::make_shared<std::string>();
  const size_t buffer_size = 256 * 1024;
  std::array<char, buffer_size> buffer;
  while (input->read(buffer.data(), buffer.size()) || input->gcount() > 0)
    contents->append(buffer.data(), input->gcount());
  if (input->bad()) {
    safe_error(error) = "Error reading: " + filename;
    return nullptr;
  }
  return contents;
}

bool build_trec(const std::vector<std::string> &text,
                std::shared_ptr<Builder> builder, std::string *error) {
  assert(builder != nullptr);
  for (auto &filename : text) {
    if (builder->verbose())
      std::cout << "Builder inhaling: " << filename << "\n";
    std::shared_ptr<std::string> contents = inhale(filename, error);
    addr p, q;
    if (!builder->add_text(*contents, &p, &q, error))
      return false;
  }
  if (builder->verbose())
    std::cout << "Finalizing...\n";
  if (!builder->finalize(error))
    return false;
  if (builder->verbose())
    std::cout << "...Finalized\n";
  return true;
}

bool build_json(const std::vector<std::string> &text,
                std::shared_ptr<Builder> builder, std::string *error) {
  return false;
}
} // namespace cottontail
