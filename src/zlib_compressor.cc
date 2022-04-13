#include "src/zlib_compressor.h"

#include <cstdint>
#include <iostream>
#include <string>

#include <zlib.h>

namespace cottontail {

size_t ZlibCompressor::crush_(char *in, size_t length, char *out,
                              size_t available) {
  z_stream defstream;
  defstream.zalloc = Z_NULL;
  defstream.zfree = Z_NULL;
  defstream.opaque = Z_NULL;
  defstream.avail_in = length;
  defstream.next_in = (Bytef *)in;
  defstream.avail_out = available;
  defstream.next_out = (Bytef *)out;
  deflateInit(&defstream, Z_BEST_COMPRESSION);
  deflate(&defstream, Z_FINISH);
  size_t actual = defstream.next_out - reinterpret_cast<unsigned char *>(out);
  deflateEnd(&defstream);
  return actual;
}

size_t ZlibCompressor::tang_(char *in, size_t length, char *out,
                             size_t available) {
  z_stream infstream;
  infstream.zalloc = Z_NULL;
  infstream.zfree = Z_NULL;
  infstream.opaque = Z_NULL;
  infstream.avail_in = length;
  infstream.next_in = (Bytef *)in;
  infstream.avail_out = available;
  infstream.next_out = (Bytef *)out;
  inflateInit(&infstream);
  inflate(&infstream, Z_NO_FLUSH);
  size_t actual = infstream.next_out - reinterpret_cast<unsigned char *>(out);
  inflateEnd(&infstream);
  return actual;
}

} // namespace cottontail
