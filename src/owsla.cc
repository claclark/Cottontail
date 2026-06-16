#include "src/owsla.h"

#include <exception>
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

std::string owsla_shard_name(const std::string &prefix, addr sequence_start,
                             addr sequence_end) {
  return prefix + "." + seq2str(sequence_start) + "." + seq2str(sequence_end);
}

namespace {

bool all_digits(const std::string &s) {
  if (s.empty())
    return false;
  for (char c : s)
    if (c < '0' || c > '9')
      return false;
  return true;
}

} // namespace

bool owsla_parse_shard_name(const std::string &name,
                            const std::string &prefix, OwslaShard *shard) {
  std::string full_prefix = prefix + ".";
  if (name.compare(0, full_prefix.size(), full_prefix) != 0)
    return false;
  size_t dot = name.find('.', full_prefix.size());
  if (dot == std::string::npos)
    return false;
  std::string start_string =
      name.substr(full_prefix.size(), dot - full_prefix.size());
  std::string end_string = name.substr(dot + 1);
  if (!all_digits(start_string) || !all_digits(end_string))
    return false;
  try {
    addr start = std::stoll(start_string);
    addr end = std::stoll(end_string);
    if (start < 0 || end < start)
      return false;
    if (shard != nullptr)
      *shard = OwslaShard(start, end, name);
    return true;
  } catch (const std::exception &) {
    return false;
  }
}

bool owsla_ranges_overlap(const OwslaShard &a, const OwslaShard &b) {
  return a.start <= b.end && b.start <= a.end;
}

bool owsla_range_contains(const OwslaShard &outer, const OwslaShard &inner) {
  return outer.start <= inner.start && inner.end <= outer.end;
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
