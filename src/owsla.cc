#include "src/owsla.h"

#include <sstream>

namespace cottontail {

const std::string cottontail_file_magic = "#COTTONTAIL\n";
const std::string transaction_tag = "\034"; // ASCII file separator
const std::string text_chunk_tag = "\035";  // ASCII group separator

const std::string hazel_blob_dictionary_magic = "COTTONTAIL_HAZEL_BLOBS\n";
const std::string hazel_idx_magic = "COTTONTAIL_HAZEL_IDX\n";
const std::string hazel_txt_magic = "COTTONTAIL_HAZEL_TXT\n";

std::string seq2str(addr sequence) {
  std::stringstream ss;
  ss.fill('0');
  ss.width(20);
  ss << sequence;
  return ss.str();
}

std::string hazel_default_name(addr sequence_start, addr sequence_end) {
  return "hazel." + seq2str(sequence_start) + "." + seq2str(sequence_end);
}

namespace {

void write_string(std::ostream *out, const std::string &value) {
  addr length = value.size();
  write_pod(out, length);
  out->write(value.data(), value.size());
}

} // namespace

std::string hazel_blob_dictionary(const std::vector<HazelBlob> &blobs) {
  std::ostringstream out(std::ios::out | std::ios::binary);
  out.write(hazel_blob_dictionary_magic.data(),
            hazel_blob_dictionary_magic.size());
  addr n = blobs.size();
  write_pod(&out, n);
  for (auto &blob : blobs) {
    write_string(&out, blob.name);
    write_pod(&out, blob.offset);
    write_pod(&out, blob.length);
  }
  return out.str();
}

} // namespace cottontail
