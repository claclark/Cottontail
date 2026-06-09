#ifndef COTTONTAIL_SRC_OWSLA_H_
#define COTTONTAIL_SRC_OWSLA_H_

#include <cstring>
#include <fstream>
#include <ostream>
#include <string>
#include <vector>

#include "src/core.h"

namespace cottontail {

extern const std::string cottontail_file_magic;
extern const std::string transaction_tag;
extern const std::string text_chunk_tag;

extern const std::string hazel_blob_dictionary_magic;
extern const std::string hazel_idx_magic;
extern const std::string hazel_txt_magic;

struct HazelBlob {
  std::string name;
  addr offset;
  addr length;
};

struct HazelPostingEntry {
  addr feature;
  addr end;
  addr count_or_p;
};

struct HazelTextEntry {
  addr raw_byte_end;
  addr compressed_byte_end;
};

std::string seq2str(addr sequence);
std::string hazel_default_name(addr sequence_start, addr sequence_end);
std::string hazel_blob_dictionary(const std::vector<HazelBlob> &blobs);

template <typename T> T read_pod(const char *data) {
  T value;
  std::memcpy(&value, data, sizeof(T));
  return value;
}

template <typename T> bool read_pod(std::fstream *in, T *value) {
  in->read(reinterpret_cast<char *>(value), sizeof(T));
  return !in->fail();
}

template <typename T> void write_pod(std::ostream *out, const T &value) {
  out->write(reinterpret_cast<const char *>(&value), sizeof(value));
}

} // namespace cottontail

#endif // COTTONTAIL_SRC_OWSLA_H_
