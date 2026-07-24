#include "src/zlib_compressor.h"

#include <cassert>
#include <cstdint>

#include <zlib.h>

namespace cottontail {

namespace {

class ZlibState final {
public:
  z_stream *reset_deflater() {
    if (deflater_initialized_) {
      if (deflateReset(&deflater_) == Z_OK)
        return &deflater_;
      deflateEnd(&deflater_);
      deflater_initialized_ = false;
    }
    deflater_ = {};
    if (deflateInit(&deflater_, Z_BEST_COMPRESSION) != Z_OK)
      return nullptr;
    deflater_initialized_ = true;
    return &deflater_;
  }

  z_stream *reset_inflater() {
    if (inflater_initialized_) {
      if (inflateReset(&inflater_) == Z_OK)
        return &inflater_;
      inflateEnd(&inflater_);
      inflater_initialized_ = false;
    }
    inflater_ = {};
    if (inflateInit(&inflater_) != Z_OK)
      return nullptr;
    inflater_initialized_ = true;
    return &inflater_;
  }

  ~ZlibState() {
    if (deflater_initialized_)
      deflateEnd(&deflater_);
    if (inflater_initialized_)
      inflateEnd(&inflater_);
  }

  ZlibState() = default;
  ZlibState(const ZlibState &) = delete;
  ZlibState &operator=(const ZlibState &) = delete;
  ZlibState(ZlibState &&) = delete;
  ZlibState &operator=(ZlibState &&) = delete;

private:
  z_stream deflater_{};
  z_stream inflater_{};
  bool deflater_initialized_ = false;
  bool inflater_initialized_ = false;
};

ZlibState &zlib_state() {
  // ZlibCompressor has one fixed recipe, so all instances on a thread can
  // safely share these resettable contexts.
  thread_local ZlibState state;
  return state;
}

} // namespace

size_t ZlibCompressor::crush_(char *in, size_t length, char *out,
                              size_t available) {
  z_stream *stream = zlib_state().reset_deflater();
  if (stream == nullptr)
    return 0;
  stream->avail_in = length;
  stream->next_in = reinterpret_cast<Bytef *>(in);
  stream->avail_out = available;
  stream->next_out = reinterpret_cast<Bytef *>(out);
  deflate(stream, Z_FINISH);
  return stream->next_out - reinterpret_cast<Bytef *>(out);
}

size_t ZlibCompressor::tang_(char *in, size_t length, char *out,
                             size_t available) {
  z_stream *stream = zlib_state().reset_inflater();
  if (stream == nullptr)
    return 0;
  stream->avail_in = length;
  stream->next_in = reinterpret_cast<Bytef *>(in);
  stream->avail_out = available;
  stream->next_out = reinterpret_cast<Bytef *>(out);
  inflate(stream, Z_NO_FLUSH);
  return stream->next_out - reinterpret_cast<Bytef *>(out);
}

addr ZlibCompressor::extra_(addr n) {
  assert(n >= 0);
  uLong source_length = n;
  uLong bound = compressBound(source_length);
  assert(bound >= source_length);
  return bound - source_length;
}

} // namespace cottontail
