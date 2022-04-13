#include "src/compressor.h"

#include <cstdint>
#include <memory>
#include <string>

#include "src/bad_compressor.h"
#include "src/core.h"
#include "src/null_compressor.h"
#include "src/post_compressor.h"
#include "src/tfdf_compressor.h"
#include "src/zlib_compressor.h"

namespace cottontail {
std::shared_ptr<Compressor> Compressor::make(const std::string &name,
                                             const std::string &recipe,
                                             std::string *error) {
  if (name == "" || name == "zlib") {
    return ZlibCompressor::make(recipe, error);
  } else if (name == "null") {
    return NullCompressor::make(recipe, error);
  } else if (name == "post") {
    return PostCompressor::make(recipe, error);
  } else if (name == "tfdf") {
    return TfdfCompressor::make(recipe, error);
  } else if (name == "bad") {
    return BadCompressor::make(recipe, error);
  } else {
    safe_set(error) = "No Compressor named: " + name;
    return nullptr;
  }
}

bool Compressor::check(const std::string &name, const std::string &recipe,
                       std::string *error) {
  if (name == "" || name == "zlib") {
    return ZlibCompressor::check(recipe, error);
  } else if (name == "null") {
    return NullCompressor::check(recipe, error);
  } else if (name == "post") {
    return PostCompressor::check(recipe, error);
  } else if (name == "tfdf") {
    return TfdfCompressor::check(recipe, error);
  } else if (name == "bad") {
    return BadCompressor::check(recipe, error);
  } else {
    safe_set(error) = "No Compressor named: " + name;
    return false;
  }
}
} // namespace cottontail
