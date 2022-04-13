#include "src/builder.h"

#include <cassert>
#include <exception>
#include <iostream>
#include <memory>
#include <regex>
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

std::shared_ptr<std::string> inhale_with_zcat(const std::string &filename,
                                              std::string *error) {
  // All this is pretty ugly..
  // ...and won't work at all on things that aren't fairly unix-like.
  // TODO: replace with a call to a library function?
  if (!has_zcat_extension(filename))
    return nullptr;
  // If zcat is not in a typical location, refuse to use it.
  std::string zcat_command;
  FILE *fp = fopen("/usr/bin/zcat", "r");
  if (fp) {
    fclose(fp);
    zcat_command = "/usr/bin/zcat " + filename;
  } else {
    fp = fopen("/bin/zcat", "r");
    assert(fp);
    fclose(fp);
    zcat_command = "/bin/zcat " + filename;
  }
  FILE *pipe = popen(zcat_command.c_str(), "r");
  assert(pipe);
  std::shared_ptr<std::string> contents = std::make_shared<std::string>();
  const size_t buffer_size = 256 * 1024;
  std::array<char, buffer_size> buffer;
  while (fgets(buffer.data(), buffer_size, pipe) != NULL)
    (*contents) += buffer.data();
  pclose(pipe);
  return contents;
}

std::shared_ptr<std::string>
inhale_assuming_uncompressed(const std::string &filename, std::string *error) {
  FILE *fp = fopen(filename.c_str(), "r");
  assert(fp);
  std::shared_ptr<std::string> contents = std::make_shared<std::string>();
  const size_t buffer_size = 256 * 1024;
  std::array<char, buffer_size> buffer;
  while (fgets(buffer.data(), buffer_size, fp) != NULL)
    (*contents) += buffer.data();
  fclose(fp);
  return contents;
}

} // namespace

std::shared_ptr<std::string> inhale(const std::string &filename,
                                    std::string *error) {
  FILE *fp = fopen(filename.c_str(), "r");
  if (!fp) {
    safe_set(error) = "Could not access: " + filename;
    return nullptr;
  }
  fclose(fp);
  std::shared_ptr<std::string> contents = inhale_with_zcat(filename, error);
  if (contents)
    return contents;
  return inhale_assuming_uncompressed(filename, error);
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
