#include "src/hazel.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

#include "src/annotator.h"
#include "src/appender.h"
#include "src/array_hopper.h"
#include "src/compressor.h"
#include "src/core.h"
#include "src/featurizer.h"
#include "src/hopper.h"
#include "src/null_annotator.h"
#include "src/null_appender.h"
#include "src/read_gate.h"
#include "src/recipe.h"
#include "src/simple_posting.h"
#include "src/stemmer.h"
#include "src/tokenizer.h"

namespace cottontail {

namespace {

class HazelFile final {
public:
  static std::shared_ptr<HazelFile> make(const std::string &filename,
                                         std::string *error) {
    std::shared_ptr<HazelFile> file =
        std::shared_ptr<HazelFile>(new HazelFile());
    file->filename_ = filename;
    file->in_.open(filename, std::ios::binary | std::ios::in);
    if (file->in_.fail()) {
      safe_error(error) = "Hazel can't open: " + filename;
      return nullptr;
    }
    return file;
  }

  bool read(addr where, char *data, addr length, std::string *error) {
    if (length < 0) {
      safe_error(error) = "Hazel got negative read length";
      return false;
    }
    if (length == 0)
      return true;
    std::lock_guard<std::mutex> lock(mutex_);
    in_.clear();
    in_.seekg(where, in_.beg);
    if (in_.fail()) {
      safe_error(error) = "Hazel can't seek in: " + filename_;
      return false;
    }
    in_.read(data, length);
    if (in_.fail()) {
      safe_error(error) = "Hazel can't read from: " + filename_;
      return false;
    }
    return true;
  }

private:
  HazelFile(){};
  std::string filename_;
  std::fstream in_;
  std::mutex mutex_;
};

bool skip_hazel_dna(std::fstream *in, std::string *error) {
  const std::string magic = cottontail_file_magic;
  std::string actual(magic.size(), '\0');
  in->read(&actual[0], actual.size());
  if (actual != magic) {
    safe_error(error) = "Hazel got bad single-file magic";
    return false;
  }
  std::string line;
  while (std::getline(*in, line))
    if (line == "")
      return true;
  safe_error(error) = "Hazel file has no DNA terminator";
  return false;
}

bool read_blob_dictionary(const std::string &filename,
                          std::map<std::string, HazelBlob> *blobs,
                          std::string *error) {
  std::fstream in(filename, std::ios::binary | std::ios::in);
  if (in.fail()) {
    safe_error(error) = "Hazel can't open: " + filename;
    return false;
  }
  if (!skip_hazel_dna(&in, error))
    return false;
  const std::string magic = hazel_blob_dictionary_magic;
  std::string actual(magic.size(), '\0');
  in.read(&actual[0], actual.size());
  if (actual != magic) {
    safe_error(error) = "Hazel got bad blob dictionary magic";
    return false;
  }
  addr count;
  if (!read_pod(&in, &count) || count < 0) {
    safe_error(error) = "Hazel got bad blob dictionary count";
    return false;
  }
  for (addr i = 0; i < count; i++) {
    addr name_length;
    if (!read_pod(&in, &name_length) || name_length < 0) {
      safe_error(error) = "Hazel got bad blob name length";
      return false;
    }
    std::string name(name_length, '\0');
    in.read(&name[0], name_length);
    HazelBlob blob;
    blob.name = name;
    if (in.fail() || !read_pod(&in, &blob.offset) ||
        !read_pod(&in, &blob.length) || blob.offset < 0 || blob.length < 0) {
      safe_error(error) = "Hazel got bad blob dictionary entry";
      return false;
    }
    (*blobs)[name] = blob;
  }
  return true;
}

bool compressor_from_recipe(const std::string &recipe,
                            const std::string &name_key,
                            const std::string &recipe_key,
                            std::shared_ptr<Compressor> *compressor,
                            std::string *error) {
  std::map<std::string, std::string> parameters;
  if (!cook(recipe, &parameters, error))
    return false;
  auto name = parameters.find(name_key);
  auto subrecipe = parameters.find(recipe_key);
  if (name == parameters.end() || subrecipe == parameters.end()) {
    safe_error(error) = "Hazel recipe has missing compressor settings";
    return false;
  }
  *compressor = Compressor::make(name->second, subrecipe->second, error);
  return *compressor != nullptr;
}

bool locate_posting(const std::vector<HazelPostingEntry> &directory,
                    addr feature, size_t *index) {
  auto it = std::lower_bound(directory.begin(), directory.end(), feature,
                             [](const HazelPostingEntry &entry, addr feature) {
                               return entry.feature < feature;
                             });
  if (it == directory.end() || it->feature != feature)
    return false;
  *index = it - directory.begin();
  return true;
}

void fill_bogus_posting(std::shared_ptr<SimplePosting> posting, addr n) {
  for (addr i = 0; i < n; i++) {
    posting->push(minfinity + 1 + i, minfinity + 2 + i, 0.0);
  }
  posting->release();
}

void fill_hazel_posting(std::shared_ptr<SimplePosting> posting,
                        std::shared_ptr<ReadGate> read_gate,
                        std::shared_ptr<SimplePostingFactory> factory,
                        addr offset, addr length, addr n) {
  std::string error;
  std::unique_ptr<char[]> bytes = read_gate->read(offset, length, &error);
  if (bytes != nullptr) {
    std::shared_ptr<SimplePosting> decoded =
        factory->posting_from_compressed_blob(bytes.get(), length, &error);
    if (decoded != nullptr && decoded->feature() == posting->feature() &&
        (addr)decoded->size() == n) {
      posting->append(decoded);
      posting->release();
      return;
    }
  }
  assert(false);
  fill_bogus_posting(posting, n);
}

template <typename Work> void async(Work work) {
  std::thread thread(work);
  thread.detach();
}

class HazelIdx;
bool hazel_feature_union(const std::vector<std::shared_ptr<HazelIdx>> &idxs,
                         std::vector<addr> *features, std::string *error);

class HazelIdx final : public Idx {
public:
  static std::shared_ptr<HazelIdx> make(const std::string &recipe,
                                        const std::string &filename,
                                        std::shared_ptr<HazelFile> file,
                                        const HazelBlob &blob,
                                        std::string *error = nullptr) {
    std::shared_ptr<HazelIdx> idx = std::shared_ptr<HazelIdx>(new HazelIdx());
    idx->therecipe_ = recipe;
    idx->file_ = file;
    idx->blob_offset_ = blob.offset;
    idx->blob_length_ = blob.length;
    idx->read_gate_ = ReadGate::make(filename, error, 16);
    if (idx->read_gate_ == nullptr)
      return nullptr;
    std::shared_ptr<Compressor> posting_compressor;
    std::shared_ptr<Compressor> fvalue_compressor;
    if (!compressor_from_recipe(recipe, "posting_compressor",
                                "posting_compressor_recipe",
                                &posting_compressor, error) ||
        !compressor_from_recipe(recipe, "fvalue_compressor",
                                "fvalue_compressor_recipe", &fvalue_compressor,
                                error))
      return nullptr;
    idx->posting_factory_ =
        SimplePostingFactory::make(posting_compressor, fvalue_compressor);
    if (!idx->load(error))
      return nullptr;
    return idx;
  }

  static bool prepare_merge(const std::vector<std::shared_ptr<HazelIdx>> &idxs,
                            const std::vector<addr> &text_lengths,
                            addr text_chunk_feature, const std::string &dst,
                            addr sequence_start, addr sequence_end,
                            std::string *error = nullptr);
  static bool write_merge(const std::vector<std::shared_ptr<HazelIdx>> &idxs,
                          const std::string &dst, addr sequence_start,
                          addr sequence_end, std::ostream *out,
                          std::string *error = nullptr);

  virtual ~HazelIdx(){};
  HazelIdx(const HazelIdx &) = delete;
  HazelIdx &operator=(const HazelIdx &) = delete;
  HazelIdx(HazelIdx &&) = delete;
  HazelIdx &operator=(HazelIdx &&) = delete;

  std::shared_ptr<SimplePosting> posting(addr feature) {
    size_t index;
    if (!locate_posting(directory_, feature, &index))
      return nullptr;
    addr start = posting_start(index);
    addr end = directory_[index].end;
    if (start == end) {
      std::shared_ptr<SimplePosting> posting =
          posting_factory_->posting_from_feature(feature);
      posting->push(directory_[index].count_or_p, directory_[index].count_or_p,
                    0.0);
      return posting;
    }
    if (start > end) {
      assert(false);
      return nullptr;
    }
    bool created;
    std::shared_ptr<SimplePosting> entry =
        cache_.get(directory_[index].feature, posting_factory_, &created);
    if (created)
      fill_hazel_posting(entry, read_gate_, posting_factory_,
                         blob_offset_ + start, end - start,
                         directory_[index].count_or_p);
    else
      entry->wait();
    return entry;
  }

  addr estimated_size() const {
    addr posting_bytes =
        directory_.empty() ? postings_start_ : directory_.back().end;
    return posting_bytes + (addr)directory_.size() * 3 * sizeof(addr);
  }

private:
  HazelIdx(){};
  std::string recipe_() final { return therecipe_; };
  std::unique_ptr<Hopper> hopper_(addr feature) final {
    size_t index;
    if (!locate_posting(directory_, feature, &index))
      return std::make_unique<EmptyHopper>();
    addr start = posting_start(index);
    addr end = directory_[index].end;
    if (start == end)
      return std::make_unique<SingletonHopper>(
          directory_[index].count_or_p, directory_[index].count_or_p, 0.0);
    if (start > end) {
      assert(false);
      return std::make_unique<EmptyHopper>();
    }
    bool created;
    std::shared_ptr<SimplePosting> entry =
        cache_.get(directory_[index].feature, posting_factory_, &created);
    if (created) {
      std::shared_ptr<ReadGate> read_gate = read_gate_;
      std::shared_ptr<SimplePostingFactory> factory = posting_factory_;
      addr offset = blob_offset_ + start;
      addr length = end - start;
      addr n = directory_[index].count_or_p;
      async([entry, read_gate, factory, offset, length, n] {
        fill_hazel_posting(entry, read_gate, factory, offset, length, n);
      });
    }
    return ArrayHopper::make(entry);
  };
  addr count_(addr feature) final {
    size_t index;
    if (!locate_posting(directory_, feature, &index))
      return 0;
    addr start = posting_start(index);
    if (start == directory_[index].end)
      return 1;
    return directory_[index].count_or_p;
  };
  addr vocab_() final { return directory_.size(); };

  addr posting_start(size_t index) const {
    return index == 0 ? postings_start_ : directory_[index - 1].end;
  }

  std::shared_ptr<SimplePosting> posting_at(size_t index, std::string *error) {
    const HazelPostingEntry &entry = directory_[index];
    addr start = posting_start(index);
    if (start == entry.end) {
      std::shared_ptr<SimplePosting> posting =
          posting_factory_->posting_from_feature(entry.feature);
      posting->push(entry.count_or_p, entry.count_or_p, 0.0);
      return posting;
    }
    if (start > entry.end) {
      safe_error(error) = "Hazel got bad idx posting boundary";
      return nullptr;
    }
    std::string bytes(entry.end - start, '\0');
    if (!file_->read(blob_offset_ + start, &bytes[0], bytes.size(), error))
      return nullptr;
    auto posting = posting_factory_->posting_from_compressed_blob(
        bytes.data(), bytes.size(), error);
    if (posting == nullptr)
      return nullptr;
    if (posting->feature() != entry.feature) {
      safe_error(error) = "Hazel posting feature differs from directory";
      return nullptr;
    }
    return posting;
  }

  bool load(std::string *error) {
    const std::string magic = hazel_idx_magic;
    postings_start_ = magic.size() + 3 * sizeof(addr);
    if (blob_length_ < postings_start_) {
      safe_error(error) = "Hazel idx blob is too short";
      return false;
    }
    std::string header(postings_start_, '\0');
    if (!file_->read(blob_offset_, &header[0], header.size(), error))
      return false;
    if (header.compare(0, magic.size(), magic) != 0) {
      safe_error(error) = "Hazel got bad idx blob magic";
      return false;
    }
    const char *p = header.data() + magic.size();
    addr directory_offset = read_pod<addr>(p);
    p += sizeof(addr);
    addr directory_length = read_pod<addr>(p);
    p += sizeof(addr);
    addr directory_count = read_pod<addr>(p);
    if (directory_offset < postings_start_ || directory_length < 0 ||
        directory_count < 0 ||
        directory_offset + directory_length > blob_length_ ||
        directory_length != directory_count * (addr)(3 * sizeof(addr))) {
      safe_error(error) = "Hazel got bad idx directory";
      return false;
    }
    std::string bytes(directory_length, '\0');
    if (!file_->read(blob_offset_ + directory_offset, &bytes[0], bytes.size(),
                     error))
      return false;
    directory_.reserve(directory_count);
    p = bytes.data();
    for (addr i = 0; i < directory_count; i++) {
      HazelPostingEntry entry;
      entry.feature = read_pod<addr>(p);
      p += sizeof(addr);
      entry.end = read_pod<addr>(p);
      p += sizeof(addr);
      entry.count_or_p = read_pod<addr>(p);
      p += sizeof(addr);
      if (entry.end < postings_start_ || entry.end > directory_offset ||
          (!directory_.empty() && entry.end < directory_.back().end)) {
        safe_error(error) = "Hazel got bad idx posting boundary";
        return false;
      }
      directory_.push_back(entry);
    }
    return true;
  }

  std::string therecipe_;
  std::shared_ptr<HazelFile> file_;
  addr blob_offset_;
  addr blob_length_;
  addr postings_start_;
  std::vector<HazelPostingEntry> directory_;
  std::shared_ptr<ReadGate> read_gate_;
  OwslaCache cache_;
  std::shared_ptr<SimplePostingFactory> posting_factory_;

  friend bool
  hazel_feature_union(const std::vector<std::shared_ptr<HazelIdx>> &idxs,
                      std::vector<addr> *features, std::string *error);
};

struct HazelMergeSegmentName {
  size_t segment = 0;
  size_t width = 0;
  OwslaShard target;
  std::string name;
};

struct HazelMergeRecord {
  addr feature = 0;
  addr end = 0;
  addr n = 0;
};

struct HazelMergeSegment {
  size_t segment = 0;
  std::string filename;
  std::vector<HazelMergeRecord> records;
};

bool hazel_parse_size(const std::string &s, size_t *value) {
  if (s.empty())
    return false;
  for (char c : s)
    if (c < '0' || c > '9')
      return false;
  try {
    size_t used = 0;
    unsigned long long parsed = std::stoull(s, &used);
    if (used != s.size() || parsed > static_cast<unsigned long long>(
                                         std::numeric_limits<size_t>::max()))
      return false;
    *value = static_cast<size_t>(parsed);
    return true;
  } catch (...) {
    return false;
  }
}

std::string hazel_segment_number(size_t segment, size_t width) {
  std::string number = std::to_string(segment);
  if (number.size() < width)
    number.insert(number.begin(), width - number.size(), '0');
  return number;
}

bool parse_hazel_merge_segment_name(const std::string &name,
                                    HazelMergeSegmentName *segment) {
  const std::string prefix = "merge.";
  if (name.compare(0, prefix.size(), prefix) != 0)
    return false;
  size_t first = name.find('.', prefix.size());
  if (first == std::string::npos)
    return false;
  size_t second = name.find('.', first + 1);
  if (second == std::string::npos ||
      name.find('.', second + 1) != std::string::npos)
    return false;

  std::string number = name.substr(prefix.size(), first - prefix.size());
  std::string start = name.substr(first + 1, second - first - 1);
  std::string end = name.substr(second + 1);
  size_t parsed_segment;
  OwslaShard target;
  if (!hazel_parse_size(number, &parsed_segment) ||
      !owsla_parse_shard_name("hazel." + start + "." + end, "hazel", &target))
    return false;
  if (start != seq2str(target.start) || end != seq2str(target.end))
    return false;
  if (segment != nullptr) {
    segment->segment = parsed_segment;
    segment->width = number.size();
    segment->target.name = hazel_default_name(target.start, target.end);
    segment->target.start = target.start;
    segment->target.end = target.end;
    segment->name = name;
  }
  return true;
}

bool normalize_hazel_merge_segment_names(
    std::vector<HazelMergeSegmentName> *segments) {
  if (segments == nullptr || segments->empty())
    return false;
  std::sort(segments->begin(), segments->end(),
            [](const HazelMergeSegmentName &a, const HazelMergeSegmentName &b) {
              return a.segment < b.segment ||
                     (a.segment == b.segment && a.name < b.name);
            });
  const size_t width = segments->front().width;
  const size_t expected_width =
      segments->size() <= 10 ? 1 : std::to_string(segments->size() - 1).size();
  const addr start = segments->front().target.start;
  const addr end = segments->front().target.end;
  if (width != expected_width)
    return false;
  for (size_t i = 0; i < segments->size(); i++) {
    const auto &segment = (*segments)[i];
    if (segment.segment != i || segment.width != width ||
        segment.target.start != start || segment.target.end != end ||
        hazel_segment_number(segment.segment, width).size() != width)
      return false;
    size_t first = segment.name.find('.', 6);
    if (first == std::string::npos ||
        segment.name.substr(6, first - 6) !=
            hazel_segment_number(segment.segment, width))
      return false;
  }
  return true;
}

std::filesystem::path hazel_merge_directory(const std::string &dst) {
  std::filesystem::path directory = std::filesystem::path(dst).parent_path();
  return directory.empty() ? std::filesystem::path(".") : directory;
}

bool hazel_discover_merge_segments(const std::string &dst, addr sequence_start,
                                   addr sequence_end,
                                   std::vector<HazelMergeSegment> *segments,
                                   bool *valid, std::string *error) {
  segments->clear();
  *valid = true;
  std::filesystem::path directory = hazel_merge_directory(dst);
  std::error_code ec;
  if (!std::filesystem::exists(directory, ec)) {
    if (ec) {
      safe_error(error) =
          "Hazel merge can't inspect directory: " + directory.string() + ": " +
          ec.message();
      return false;
    }
    return true;
  }

  std::vector<HazelMergeSegmentName> found;
  std::filesystem::directory_iterator it(directory, ec);
  std::filesystem::directory_iterator end;
  if (ec) {
    safe_error(error) =
        "Hazel merge can't list directory: " + directory.string() + ": " +
        ec.message();
    return false;
  }
  for (; it != end; it.increment(ec)) {
    if (ec) {
      safe_error(error) =
          "Hazel merge can't list directory: " + directory.string() + ": " +
          ec.message();
      return false;
    }
    HazelMergeSegmentName parsed;
    std::string name = it->path().filename().string();
    if (!parse_hazel_merge_segment_name(name, &parsed) ||
        parsed.target.start != sequence_start ||
        parsed.target.end != sequence_end)
      continue;
    found.push_back(parsed);
  }
  if (found.empty())
    return true;
  if (!normalize_hazel_merge_segment_names(&found)) {
    *valid = false;
    for (auto &item : found)
      segments->push_back({item.segment, (directory / item.name).string(), {}});
    return true;
  }
  for (auto &item : found)
    segments->push_back({item.segment, (directory / item.name).string(), {}});
  return true;
}

bool hazel_file_size(const std::string &filename, addr *size) {
  std::fstream in(filename, std::ios::binary | std::ios::in | std::ios::ate);
  if (in.fail())
    return false;
  std::streampos end = in.tellg();
  if (end < 0)
    return false;
  *size = (addr)end;
  return true;
}

bool hazel_reset_file(const std::string &filename, std::string *error) {
  std::fstream out(filename,
                   std::ios::binary | std::ios::out | std::ios::trunc);
  if (out.fail()) {
    safe_error(error) = "Hazel merge can't create posting log: " + filename;
    return false;
  }
  out.close();
  if (out.fail()) {
    safe_error(error) = "Hazel merge can't close posting log: " + filename;
    return false;
  }
  return true;
}

bool hazel_truncate_file(const std::string &filename, addr size,
                         std::string *error) {
  if (size < 0) {
    safe_error(error) = "Hazel merge got negative posting-log size";
    return false;
  }
  if (truncate(filename.c_str(), size) != 0) {
    safe_error(error) = "Hazel merge can't truncate posting log: " + filename;
    return false;
  }
  return true;
}

bool hazel_remove_merge_segments(const std::string &dst, addr sequence_start,
                                 addr sequence_end, std::string *error) {
  std::filesystem::path directory = hazel_merge_directory(dst);
  std::error_code ec;
  if (!std::filesystem::exists(directory, ec)) {
    if (ec) {
      safe_error(error) =
          "Hazel merge can't inspect directory: " + directory.string() + ": " +
          ec.message();
      return false;
    }
    return true;
  }
  std::filesystem::directory_iterator it(directory, ec);
  std::filesystem::directory_iterator end;
  if (ec) {
    safe_error(error) =
        "Hazel merge can't list directory: " + directory.string() + ": " +
        ec.message();
    return false;
  }
  std::vector<std::pair<size_t, std::filesystem::path>> found;
  for (; it != end; it.increment(ec)) {
    if (ec) {
      safe_error(error) =
          "Hazel merge can't list directory: " + directory.string() + ": " +
          ec.message();
      return false;
    }
    HazelMergeSegmentName segment;
    if (!parse_hazel_merge_segment_name(it->path().filename().string(),
                                        &segment) ||
        segment.target.start != sequence_start ||
        segment.target.end != sequence_end)
      continue;
    found.push_back({segment.segment, it->path()});
  }
  std::sort(found.begin(), found.end(),
            [](const auto &a, const auto &b) { return a.first < b.first; });
  for (auto &item : found) {
    std::error_code remove_error;
    std::filesystem::remove(item.second, remove_error);
    if (remove_error) {
      safe_error(error) =
          "Hazel merge can't remove posting log: " + item.second.string() +
          ": " + remove_error.message();
      return false;
    }
  }
  return true;
}

bool hazel_create_merge_segments(const std::string &dst, addr sequence_start,
                                 addr sequence_end, size_t count,
                                 std::vector<HazelMergeSegment> *segments,
                                 std::string *error) {
  if (count == 0) {
    safe_error(error) = "Hazel merge needs at least one posting log";
    return false;
  }
  std::filesystem::path directory = hazel_merge_directory(dst);
  for (size_t i = count; i > 0; i--) {
    std::string name =
        hazel_merge_segment_name(i - 1, count, sequence_start, sequence_end);
    if (!hazel_reset_file((directory / name).string(), error))
      return false;
  }
  bool valid;
  if (!hazel_discover_merge_segments(dst, sequence_start, sequence_end,
                                     segments, &valid, error))
    return false;
  if (!valid || segments->size() != count) {
    safe_error(error) = "Hazel merge created inconsistent posting logs";
    return false;
  }
  return true;
}

bool hazel_scan_merge_segment(HazelMergeSegment *segment, bool *valid,
                              std::string *error) {
  *valid = true;
  segment->records.clear();
  addr size;
  if (!hazel_file_size(segment->filename, &size)) {
    safe_error(error) =
        "Hazel merge can't inspect posting log: " + segment->filename;
    return false;
  }
  std::fstream in(segment->filename, std::ios::binary | std::ios::in);
  if (in.fail()) {
    safe_error(error) =
        "Hazel merge can't read posting log: " + segment->filename;
    return false;
  }

  addr offset = 0;
  addr truncate_at = size;
  while (offset < size) {
    addr available = size - offset;
    if (available < static_cast<addr>(sizeof(PstRecord))) {
      truncate_at = offset;
      break;
    }
    PstRecord record;
    in.clear();
    in.seekg(offset, in.beg);
    in.read(reinterpret_cast<char *>(&record), sizeof(record));
    if (in.fail()) {
      safe_error(error) =
          "Hazel merge can't read posting log: " + segment->filename;
      return false;
    }
    if (record.feature < null_feature || record.n < 0 || record.pst < 0 ||
        record.qst < 0 || record.fst < 0 ||
        record.n > maxfinity / static_cast<addr>(sizeof(addr)) ||
        record.n > maxfinity / static_cast<addr>(sizeof(fval)) ||
        (record.n > 0 && record.pst == 0) ||
        (record.n == 0 &&
         (record.pst != 0 || record.qst != 0 || record.fst != 0)) ||
        (!segment->records.empty() &&
         record.feature <= segment->records.back().feature)) {
      *valid = false;
      return true;
    }

    addr remaining = available - sizeof(PstRecord);
    bool partial = false;
    for (addr length : {record.pst, record.qst, record.fst}) {
      if (length > remaining) {
        partial = true;
        break;
      }
      remaining -= length;
    }
    if (partial) {
      truncate_at = offset;
      break;
    }
    addr end = size - remaining;
    segment->records.push_back({record.feature, end, record.n});
    offset = end;
  }
  in.close();
  if (in.fail()) {
    safe_error(error) =
        "Hazel merge can't close posting log: " + segment->filename;
    return false;
  }
  if (truncate_at != size &&
      !hazel_truncate_file(segment->filename, truncate_at, error))
    return false;
  return true;
}

bool hazel_scan_merge_segments(std::vector<HazelMergeSegment> *segments,
                               bool *valid, std::string *error) {
  *valid = true;
  for (auto &segment : *segments) {
    bool segment_valid;
    if (!hazel_scan_merge_segment(&segment, &segment_valid, error))
      return false;
    if (!segment_valid) {
      *valid = false;
      return true;
    }
  }
  return true;
}

bool hazel_feature_union(const std::vector<std::shared_ptr<HazelIdx>> &idxs,
                         std::vector<addr> *features, std::string *error) {
  features->clear();
  for (auto &idx : idxs)
    for (size_t i = 1; i < idx->directory_.size(); i++)
      if (idx->directory_[i].feature <= idx->directory_[i - 1].feature) {
        safe_error(error) = "Hazel merge got unordered source directory";
        return false;
      }

  std::vector<size_t> positions(idxs.size(), 0);
  for (;;) {
    bool found = false;
    addr next = 0;
    for (size_t i = 0; i < idxs.size(); i++)
      if (positions[i] < idxs[i]->directory_.size() &&
          (!found || idxs[i]->directory_[positions[i]].feature < next)) {
        found = true;
        next = idxs[i]->directory_[positions[i]].feature;
      }
    if (!found)
      return true;
    features->push_back(next);
    for (size_t i = 0; i < idxs.size(); i++)
      if (positions[i] < idxs[i]->directory_.size() &&
          idxs[i]->directory_[positions[i]].feature == next)
        positions[i]++;
  }
}

struct HazelMergeCursor {
  addr feature;
  size_t segment;
  size_t record;
};

struct HazelMergeCursorLater {
  bool operator()(const HazelMergeCursor &a, const HazelMergeCursor &b) const {
    return a.feature > b.feature ||
           (a.feature == b.feature &&
            (a.segment > b.segment ||
             (a.segment == b.segment && a.record > b.record)));
  }
};

bool hazel_completed_features(const std::vector<HazelMergeSegment> &segments,
                              const std::vector<addr> &source_features,
                              addr text_chunk_feature,
                              std::vector<addr> *completed, bool *valid) {
  completed->clear();
  *valid = true;
  std::priority_queue<HazelMergeCursor, std::vector<HazelMergeCursor>,
                      HazelMergeCursorLater>
      queue;
  for (size_t i = 0; i < segments.size(); i++)
    if (!segments[i].records.empty())
      queue.push({segments[i].records[0].feature, i, 0});

  bool have_previous = false;
  addr previous = 0;
  size_t source = 0;
  while (!queue.empty()) {
    HazelMergeCursor cursor = queue.top();
    queue.pop();
    const HazelMergeRecord &record =
        segments[cursor.segment].records[cursor.record];
    if (have_previous && record.feature == previous) {
      *valid = false;
      return true;
    }
    have_previous = true;
    previous = record.feature;
    while (source < source_features.size() &&
           source_features[source] < record.feature)
      source++;
    if (source < source_features.size() &&
        source_features[source] == record.feature) {
      if ((record.feature == null_feature ||
           record.feature == text_chunk_feature) &&
          record.n == 0) {
        *valid = false;
        return true;
      }
      completed->push_back(record.feature);
    } else if (record.n != 0) {
      *valid = false;
      return true;
    }
    size_t next = cursor.record + 1;
    if (next < segments[cursor.segment].records.size())
      queue.push({segments[cursor.segment].records[next].feature,
                  cursor.segment, next});
  }
  return true;
}

size_t hazel_completed_prefix(const std::vector<addr> &features,
                              const std::vector<addr> &completed) {
  size_t feature = 0;
  size_t done = 0;
  while (feature < features.size() && done < completed.size()) {
    if (completed[done] < features[feature]) {
      done++;
    } else if (completed[done] == features[feature]) {
      done++;
      feature++;
    } else {
      break;
    }
  }
  return feature;
}

bool hazel_truncate_active_segments(std::vector<HazelMergeSegment> *segments,
                                    size_t active,
                                    const std::vector<addr> &features,
                                    size_t completed_prefix,
                                    std::string *error) {
  bool have_prefix = completed_prefix > 0;
  addr last = have_prefix ? features[completed_prefix - 1] : 0;
  for (size_t i = 0; i < active; i++) {
    size_t keep = 0;
    if (have_prefix)
      while (keep < (*segments)[i].records.size() &&
             (*segments)[i].records[keep].feature <= last)
        keep++;
    addr size = keep == 0 ? 0 : (*segments)[i].records[keep - 1].end;
    if (!hazel_truncate_file((*segments)[i].filename, size, error))
      return false;
    (*segments)[i].records.resize(keep);
  }
  return true;
}

bool hazel_read_merge_record(const HazelMergeSegment &segment, size_t record,
                             std::vector<char> *bytes, std::string *error) {
  if (record >= segment.records.size()) {
    safe_error(error) = "Hazel merge got bad posting-log record";
    return false;
  }
  addr start = record == 0 ? 0 : segment.records[record - 1].end;
  addr end = segment.records[record].end;
  if (end < start ||
      static_cast<uint64_t>(end - start) >
          static_cast<uint64_t>(std::numeric_limits<std::streamsize>::max()) ||
      static_cast<uint64_t>(end - start) >
          static_cast<uint64_t>(std::numeric_limits<size_t>::max())) {
    safe_error(error) = "Hazel merge got bad posting-log record";
    return false;
  }
  bytes->resize(static_cast<size_t>(end - start));
  std::fstream in(segment.filename, std::ios::binary | std::ios::in);
  if (in.fail()) {
    safe_error(error) =
        "Hazel merge can't read posting log: " + segment.filename;
    return false;
  }
  in.seekg(start, in.beg);
  in.read(bytes->data(), static_cast<std::streamsize>(bytes->size()));
  if (in.fail()) {
    safe_error(error) =
        "Hazel merge got truncated posting log: " + segment.filename;
    return false;
  }
  return true;
}

bool hazel_append_merge_record(const std::string &filename,
                               std::shared_ptr<SimplePosting> posting,
                               std::string *error) {
  if (posting == nullptr) {
    safe_error(error) = "Hazel merge got null posting-log record";
    return false;
  }
  std::ofstream out(filename, std::ios::binary | std::ios::app);
  if (out.fail()) {
    safe_error(error) = "Hazel merge can't append posting log: " + filename;
    return false;
  }
  posting->write(&out);
  out.close();
  if (out.fail()) {
    safe_error(error) = "Hazel merge can't close posting log: " + filename;
    return false;
  }
  return true;
}

bool hazel_assemble_idx(const std::vector<HazelMergeSegment> &segments,
                        std::shared_ptr<SimplePostingFactory> factory,
                        std::ostream *out, std::string *error) {
  addr blob_start = static_cast<addr>(out->tellp());
  out->write(hazel_idx_magic.data(), hazel_idx_magic.size());
  write_pod<addr>(out, 0);
  write_pod<addr>(out, 0);
  write_pod<addr>(out, 0);
  if (out->fail()) {
    safe_error(error) = "Hazel merge failed to write idx header";
    return false;
  }

  std::vector<std::unique_ptr<std::ifstream>> inputs;
  inputs.reserve(segments.size());
  for (auto &segment : segments) {
    auto input = std::make_unique<std::ifstream>(
        segment.filename, std::ios::binary | std::ios::in);
    if (input->fail()) {
      safe_error(error) =
          "Hazel merge can't read posting log: " + segment.filename;
      return false;
    }
    inputs.push_back(std::move(input));
  }

  std::priority_queue<HazelMergeCursor, std::vector<HazelMergeCursor>,
                      HazelMergeCursorLater>
      queue;
  for (size_t i = 0; i < segments.size(); i++)
    if (!segments[i].records.empty())
      queue.push({segments[i].records[0].feature, i, 0});

  std::vector<HazelPostingEntry> directory;
  std::vector<char> record_bytes;
  std::vector<char> copy_buffer(1 << 20);
  while (!queue.empty()) {
    HazelMergeCursor cursor = queue.top();
    queue.pop();
    const HazelMergeSegment &segment = segments[cursor.segment];
    const HazelMergeRecord &record = segment.records[cursor.record];
    addr start =
        cursor.record == 0 ? 0 : segment.records[cursor.record - 1].end;
    addr length = record.end - start;
    std::ifstream &input = *inputs[cursor.segment];
    input.clear();
    input.seekg(start, input.beg);
    PstRecord header;
    input.read(reinterpret_cast<char *>(&header), sizeof(header));
    if (input.fail() || header.feature != record.feature ||
        header.n != record.n) {
      safe_error(error) =
          "Hazel merge got changed posting log: " + segment.filename;
      return false;
    }

    if (header.n != 0) {
      HazelPostingEntry entry;
      entry.feature = header.feature;
      entry.end = static_cast<addr>(out->tellp()) - blob_start;
      entry.count_or_p = header.n;
      if (header.n == 1 && header.qst == 0 && header.fst == 0) {
        if (length < static_cast<addr>(sizeof(PstRecord)) ||
            static_cast<uint64_t>(length) >
                static_cast<uint64_t>(
                    std::numeric_limits<std::streamsize>::max()) ||
            static_cast<uint64_t>(length) >
                static_cast<uint64_t>(std::numeric_limits<size_t>::max())) {
          safe_error(error) = "Hazel merge got bad singleton posting record";
          return false;
        }
        record_bytes.resize(static_cast<size_t>(length));
        input.clear();
        input.seekg(start, input.beg);
        input.read(record_bytes.data(),
                   static_cast<std::streamsize>(record_bytes.size()));
        if (input.fail()) {
          safe_error(error) =
              "Hazel merge got truncated posting log: " + segment.filename;
          return false;
        }
        auto posting = factory->posting_from_compressed_blob(
            record_bytes.data(), record_bytes.size(), error);
        addr p, q;
        fval v;
        if (posting == nullptr || posting->size() != 1 ||
            !posting->get(0, &p, &q, &v) || p != q || v != 0.0) {
          safe_error(error) = "Hazel merge got bad singleton posting record";
          return false;
        }
        entry.count_or_p = p;
      } else {
        input.clear();
        input.seekg(start, input.beg);
        addr remaining = length;
        while (remaining > 0) {
          addr amount =
              std::min<addr>(remaining, static_cast<addr>(copy_buffer.size()));
          input.read(copy_buffer.data(), amount);
          if (input.fail()) {
            safe_error(error) =
                "Hazel merge got truncated posting log: " + segment.filename;
            return false;
          }
          out->write(copy_buffer.data(), amount);
          if (out->fail()) {
            safe_error(error) = "Hazel merge failed to copy posting record";
            return false;
          }
          remaining -= amount;
        }
        entry.end = static_cast<addr>(out->tellp()) - blob_start;
      }
      directory.push_back(entry);
    }

    size_t next = cursor.record + 1;
    if (next < segment.records.size())
      queue.push({segment.records[next].feature, cursor.segment, next});
  }

  addr directory_offset = static_cast<addr>(out->tellp()) - blob_start;
  for (auto &entry : directory) {
    write_pod(out, entry.feature);
    write_pod(out, entry.end);
    write_pod(out, entry.count_or_p);
  }
  addr directory_length =
      static_cast<addr>(out->tellp()) - blob_start - directory_offset;
  addr directory_count = directory.size();
  addr blob_end = static_cast<addr>(out->tellp());
  out->seekp(blob_start + hazel_idx_magic.size());
  write_pod(out, directory_offset);
  write_pod(out, directory_length);
  write_pod(out, directory_count);
  out->seekp(blob_end);
  if (out->fail()) {
    safe_error(error) = "Hazel merge failed to write idx directory";
    return false;
  }
  return true;
}

bool HazelIdx::prepare_merge(const std::vector<std::shared_ptr<HazelIdx>> &idxs,
                             const std::vector<addr> &text_lengths,
                             addr text_chunk_feature, const std::string &dst,
                             addr sequence_start, addr sequence_end,
                             std::string *error) {
  if (idxs.size() < 2) {
    safe_error(error) = "HazelIdx merge needs at least two indexes";
    return false;
  }
  if (text_lengths.size() != idxs.size()) {
    safe_error(error) = "HazelIdx merge got wrong text length count";
    return false;
  }
  if (text_chunk_feature == null_feature) {
    safe_error(error) = "HazelIdx merge got bad text chunk feature";
    return false;
  }
  if (dst == "" || sequence_start < 0 || sequence_end < sequence_start) {
    safe_error(error) = "HazelIdx merge got bad posting-log target";
    return false;
  }
  for (size_t i = 0; i < idxs.size(); i++) {
    if (idxs[i] == nullptr || idxs[i]->posting_factory_ == nullptr) {
      safe_error(error) = "HazelIdx merge got null index";
      return false;
    }
    if (text_lengths[i] < 0) {
      safe_error(error) = "HazelIdx merge got bad text length";
      return false;
    }
    if (idxs[i]->recipe() != idxs[0]->recipe()) {
      safe_error(error) = "HazelIdx merge got incompatible recipes";
      return false;
    }
  }

  std::shared_ptr<SimplePostingFactory> factory = idxs[0]->posting_factory_;
  std::vector<addr> text_bases;
  text_bases.reserve(text_lengths.size());
  addr text_base = 0;
  for (addr length : text_lengths) {
    if (length > maxfinity - text_base) {
      safe_error(error) = "HazelIdx merge got overflowing text length";
      return false;
    }
    text_bases.push_back(text_base);
    text_base += length;
  }

  auto postings_for_feature =
      [&](addr feature, std::vector<std::shared_ptr<SimplePosting>> *postings,
          std::string *error) {
        postings->clear();
        for (auto &idx : idxs) {
          size_t index;
          if (locate_posting(idx->directory_, feature, &index)) {
            auto posting = idx->posting_at(index, error);
            if (posting == nullptr)
              return false;
            postings->push_back(posting);
          }
        }
        return true;
      };

  auto text_chunk_posting = [&]() -> std::shared_ptr<SimplePosting> {
    std::shared_ptr<SimplePosting> posting =
        factory->posting_from_feature(text_chunk_feature);
    for (size_t i = 0; i < idxs.size(); i++) {
      size_t index;
      if (!locate_posting(idxs[i]->directory_, text_chunk_feature, &index))
        continue;
      std::string posting_error;
      auto source = idxs[i]->posting_at(index, &posting_error);
      if (source == nullptr) {
        safe_error(error) = posting_error;
        return nullptr;
      }
      addr p, q;
      fval v;
      for (size_t j = 0; j < source->size(); j++) {
        source->get(j, &p, &q, &v);
        posting->push(p, q, addr2fval(fval2addr(v) + text_bases[i]));
      }
    }
    if (posting->size() == 0)
      return nullptr;
    return posting;
  };

  std::vector<addr> features;
  if (!hazel_feature_union(idxs, &features, error))
    return false;
  size_t permitted = idxs.size() == 2 ? 1 : allowed_threads(0);
  size_t desired_segments =
      std::min(permitted, std::max<size_t>(1, features.size()));

  std::vector<HazelMergeSegment> segments;
  bool names_valid;
  if (!hazel_discover_merge_segments(dst, sequence_start, sequence_end,
                                     &segments, &names_valid, error))
    return false;
  if (!names_valid) {
    if (!hazel_remove_merge_segments(dst, sequence_start, sequence_end, error))
      return false;
    segments.clear();
  }
  if (segments.empty() &&
      !hazel_create_merge_segments(dst, sequence_start, sequence_end,
                                   desired_segments, &segments, error))
    return false;

  bool records_valid;
  if (!hazel_scan_merge_segments(&segments, &records_valid, error))
    return false;
  std::vector<addr> completed;
  bool completion_valid = false;
  if (records_valid &&
      !hazel_completed_features(segments, features, text_chunk_feature,
                                &completed, &completion_valid))
    return false;
  if (!records_valid || !completion_valid) {
    if (!hazel_remove_merge_segments(dst, sequence_start, sequence_end, error))
      return false;
    segments.clear();
    if (!hazel_create_merge_segments(dst, sequence_start, sequence_end,
                                     desired_segments, &segments, error) ||
        !hazel_scan_merge_segments(&segments, &records_valid, error))
      return false;
    if (!records_valid) {
      safe_error(error) = "Hazel merge created invalid posting logs";
      return false;
    }
    completed.clear();
  }

  size_t active = std::min(permitted, segments.size());
  if (active == 0) {
    safe_error(error) = "Hazel merge has no active posting logs";
    return false;
  }
  size_t prefix = hazel_completed_prefix(features, completed);
  if (!hazel_truncate_active_segments(&segments, active, features, prefix,
                                      error) ||
      !hazel_completed_features(segments, features, text_chunk_feature,
                                &completed, &completion_valid))
    return false;
  if (!completion_valid) {
    safe_error(error) = "Hazel merge got inconsistent recovered posting logs";
    return false;
  }

  bool source_has_null =
      std::binary_search(features.begin(), features.end(), null_feature);
  std::shared_ptr<SimplePosting> exclude;
  if (source_has_null &&
      std::binary_search(completed.begin(), completed.end(), null_feature)) {
    bool found = false;
    std::vector<char> bytes;
    for (auto &segment : segments)
      for (size_t i = 0; i < segment.records.size(); i++)
        if (segment.records[i].feature == null_feature) {
          if (!hazel_read_merge_record(segment, i, &bytes, error))
            return false;
          exclude = factory->posting_from_compressed_blob(bytes.data(),
                                                          bytes.size(), error);
          if (exclude == nullptr ||
              exclude->size() != static_cast<size_t>(segment.records[i].n)) {
            safe_error(error) =
                "Hazel merge got bad recovered exclusion posting";
            return false;
          }
          found = true;
          break;
        }
    if (!found) {
      safe_error(error) = "Hazel merge lost recovered exclusion posting";
      return false;
    }
  } else if (source_has_null) {
    std::vector<std::shared_ptr<SimplePosting>> postings;
    if (!postings_for_feature(null_feature, &postings, error))
      return false;
    exclude = factory->posting_from_merge(postings);
    if (exclude == nullptr || exclude->size() == 0) {
      safe_error(error) = "Hazel merge got empty exclusion posting";
      return false;
    }
    if (!hazel_append_merge_record(segments[0].filename, exclude, error) ||
        !hazel_scan_merge_segment(&segments[0], &records_valid, error))
      return false;
    if (!records_valid) {
      safe_error(error) = "Hazel merge wrote invalid exclusion posting";
      return false;
    }
    completed.insert(completed.begin(), null_feature);
  }

  bool text_done = std::binary_search(completed.begin(), completed.end(),
                                      text_chunk_feature);
  std::shared_ptr<SimplePosting> merged_text_chunks;
  if (std::binary_search(features.begin(), features.end(),
                         text_chunk_feature) &&
      !text_done) {
    merged_text_chunks = text_chunk_posting();
    if (merged_text_chunks == nullptr) {
      if (safe_set(error) == "")
        safe_error(error) = "Hazel merge got empty text-chunk posting";
      return false;
    }
  }

  struct FeatureTask {
    addr feature = 0;
    std::vector<std::pair<size_t, size_t>> sources;
  };
  std::mutex claim_lock;
  std::vector<size_t> source_positions(idxs.size(), 0);
  size_t feature_position = 0;
  size_t completed_position = 0;
  auto claim = [&](FeatureTask *task) {
    std::lock_guard<std::mutex> lock(claim_lock);
    while (feature_position < features.size()) {
      addr feature = features[feature_position++];
      task->feature = feature;
      task->sources.clear();
      for (size_t i = 0; i < idxs.size(); i++) {
        while (source_positions[i] < idxs[i]->directory_.size() &&
               idxs[i]->directory_[source_positions[i]].feature < feature)
          source_positions[i]++;
        if (source_positions[i] < idxs[i]->directory_.size() &&
            idxs[i]->directory_[source_positions[i]].feature == feature) {
          task->sources.push_back({i, source_positions[i]});
          source_positions[i]++;
        }
      }
      while (completed_position < completed.size() &&
             completed[completed_position] < feature)
        completed_position++;
      if (completed_position < completed.size() &&
          completed[completed_position] == feature) {
        completed_position++;
        continue;
      }
      return true;
    }
    return false;
  };

  size_t unfinished = features.size() - completed.size();
  size_t workers = std::min(active, unfinished);
  std::atomic<bool> failed(false);
  std::mutex failure_lock;
  std::string failure;
  auto fail = [&](const std::string &message) {
    bool expected = false;
    if (failed.compare_exchange_strong(expected, true)) {
      std::lock_guard<std::mutex> lock(failure_lock);
      failure = message;
    }
  };

  std::vector<std::unique_ptr<std::ofstream>> outputs;
  outputs.reserve(workers);
  for (size_t i = 0; i < workers; i++) {
    auto output = std::make_unique<std::ofstream>(
        segments[i].filename, std::ios::binary | std::ios::app);
    if (output->fail()) {
      safe_error(error) =
          "Hazel merge can't append posting log: " + segments[i].filename;
      return false;
    }
    outputs.push_back(std::move(output));
  }

  std::vector<std::thread> threads;
  threads.reserve(workers);
  try {
    for (size_t worker = 0; worker < workers; worker++)
      threads.emplace_back([&, worker] {
        try {
          FeatureTask task;
          task.sources.reserve(idxs.size());
          std::vector<std::shared_ptr<SimplePosting>> postings;
          postings.reserve(idxs.size());
          while (!failed.load() && claim(&task)) {
            std::shared_ptr<SimplePosting> posting;
            if (task.feature == null_feature) {
              fail("Hazel merge scheduled null feature after serial setup");
              break;
            } else if (task.feature == text_chunk_feature) {
              posting = merged_text_chunks;
              if (posting == nullptr) {
                fail("Hazel merge lost precomputed text-chunk posting");
                break;
              }
            } else {
              postings.clear();
              for (auto &source : task.sources) {
                std::string posting_error;
                auto input = idxs[source.first]->posting_at(source.second,
                                                            &posting_error);
                if (input == nullptr) {
                  fail(posting_error == ""
                           ? "Hazel merge can't read source posting"
                           : posting_error);
                  break;
                }
                postings.push_back(input);
              }
              if (failed.load())
                break;
              posting = factory->posting_from_merge(postings, exclude);
              if (posting == nullptr)
                posting = factory->posting_from_feature(task.feature);
            }
            posting->write(outputs[worker].get());
            if (outputs[worker]->fail()) {
              fail("Hazel merge failed to append posting log");
              break;
            }
          }
        } catch (const std::exception &exception) {
          fail("Hazel merge posting worker failed: " +
               std::string(exception.what()));
        } catch (...) {
          fail("Hazel merge posting worker failed");
        }
        outputs[worker]->close();
        if (outputs[worker]->fail())
          fail("Hazel merge failed to close posting log");
      });
  } catch (const std::exception &exception) {
    fail("Hazel merge can't start posting worker: " +
         std::string(exception.what()));
  } catch (...) {
    fail("Hazel merge can't start posting worker");
  }
  for (auto &thread : threads)
    thread.join();
  if (failed.load()) {
    std::lock_guard<std::mutex> lock(failure_lock);
    safe_error(error) = failure;
    return false;
  }

  if (!hazel_scan_merge_segments(&segments, &records_valid, error))
    return false;
  if (!records_valid ||
      !hazel_completed_features(segments, features, text_chunk_feature,
                                &completed, &completion_valid))
    return false;
  if (!completion_valid || completed != features) {
    safe_error(error) = "Hazel merge left incomplete posting logs";
    return false;
  }
  return true;
}

bool HazelIdx::write_merge(const std::vector<std::shared_ptr<HazelIdx>> &idxs,
                           const std::string &dst, addr sequence_start,
                           addr sequence_end, std::ostream *out,
                           std::string *error) {
  if (idxs.empty() || idxs[0] == nullptr ||
      idxs[0]->posting_factory_ == nullptr || out == nullptr) {
    safe_error(error) = "HazelIdx merge got incomplete assembly state";
    return false;
  }
  std::vector<HazelMergeSegment> segments;
  bool names_valid;
  if (!hazel_discover_merge_segments(dst, sequence_start, sequence_end,
                                     &segments, &names_valid, error))
    return false;
  if (!names_valid || segments.empty()) {
    safe_error(error) = "Hazel merge lost prepared posting logs";
    return false;
  }
  bool records_valid;
  if (!hazel_scan_merge_segments(&segments, &records_valid, error))
    return false;
  if (!records_valid) {
    safe_error(error) = "Hazel merge got invalid prepared posting logs";
    return false;
  }
  return hazel_assemble_idx(segments, idxs[0]->posting_factory_, out, error);
}

struct HazelTextCacheEntry {
  std::unique_ptr<char[]> bytes;
  std::atomic<bool> present{false};
};

class HazelTxt final : public Txt {
public:
  static std::shared_ptr<HazelTxt>
  make(const std::string &recipe, const std::string &filename, addr blob_offset,
       addr blob_length, std::shared_ptr<Tokenizer> tokenizer,
       std::unique_ptr<Hopper> hopper, std::string *error = nullptr) {
    std::shared_ptr<HazelTxt> txt = std::shared_ptr<HazelTxt>(new HazelTxt());
    txt->therecipe_ = recipe;
    txt->read_gate_ = ReadGate::make(filename, error, 16);
    if (txt->read_gate_ == nullptr)
      return nullptr;
    txt->tokenizer_ = tokenizer;
    txt->hopper_ = std::move(hopper);
    if (!compressor_from_recipe(recipe, "compressor", "compressor_recipe",
                                &txt->compressor_, error) ||
        !txt->load(blob_offset, blob_length, error))
      return nullptr;
    txt->load_token_range();
    return txt;
  }

  virtual ~HazelTxt(){};
  HazelTxt(const HazelTxt &) = delete;
  HazelTxt &operator=(const HazelTxt &) = delete;
  HazelTxt(HazelTxt &&) = delete;
  HazelTxt &operator=(HazelTxt &&) = delete;

  static bool merge(const std::vector<std::shared_ptr<HazelTxt>> &txts,
                    std::ostream *out, std::string *error = nullptr);
  addr raw_text_length() const { return raw_text_length_; }
  addr estimated_size() const { return estimated_size_; }

private:
  HazelTxt(){};
  std::string name_() final { return "hazel"; };
  std::string recipe_() final { return therecipe_; };
  std::shared_ptr<Txt> clone_(std::string *error) final {
    safe_error(error) = "HazelTxt should not be cloned";
    return nullptr;
  }
  std::string translate_(addr p, addr q) final {
    if (token_start_ == maxfinity)
      return "";
    if (p < token_start_)
      p = token_start_;
    if (q > token_end_)
      q = token_end_;
    if (p == maxfinity || q < p)
      return "";

    addr p0, q0, left_anchor_byte;
    addr p1, q1, right_anchor_byte;
    {
      std::lock_guard<std::mutex> lock(hopper_lock_);
      hopper_->rho(p, &p0, &q0, &left_anchor_byte);
      if (p0 == maxfinity || p0 > q)
        return "";
      hopper_->ohr(q, &p1, &q1, &right_anchor_byte);
    }
    if (p1 == minfinity || p1 == maxfinity)
      return "";
    if (left_anchor_byte < 0 || left_anchor_byte > raw_text_length_ ||
        right_anchor_byte < 0 || right_anchor_byte > raw_text_length_)
      return "";

    addr cover_byte_start = left_anchor_byte;
    addr cover_byte_end = q > q1 ? raw_text_length_ : right_anchor_byte;
    if (cover_byte_start > cover_byte_end || map_.empty())
      return "";

    size_t first_chunk = chunk_containing(left_anchor_byte);
    size_t last_chunk;
    if (q > q1)
      last_chunk = map_.size() - 1;
    else
      last_chunk = chunk_containing(right_anchor_byte);
    if (first_chunk >= map_.size() || last_chunk >= map_.size() ||
        first_chunk > last_chunk)
      return "";
    addr window_byte_start =
        first_chunk == 0 ? 0 : map_[first_chunk - 1].raw_byte_end;
    addr window_byte_end = map_[last_chunk].raw_byte_end;
    if (cover_byte_start < window_byte_start ||
        cover_byte_end > window_byte_end)
      return "";
    std::string cover = raw_bytes(window_byte_start, window_byte_end);
    if ((addr)cover.size() != window_byte_end - window_byte_start)
      return "";

    const char *base = cover.data();
    const char *limit = base + cover.size();
    const char *start = base + (left_anchor_byte - window_byte_start);
    if (p0 < p)
      start = tokenizer_->skip(start, limit - start, p - p0);
    else
      p = p0;

    const char *end;
    if (q > q1) {
      end = limit;
    } else if (q0 == q1) {
      end = tokenizer_->skip(start, limit - start, q - p + 1);
    } else {
      addr offset = right_anchor_byte - window_byte_start;
      if (offset < 0 || offset > (addr)cover.size())
        return "";
      end =
          tokenizer_->skip(base + offset, limit - (base + offset), q - p1 + 1);
    }
    if (start < base || end < start || end > limit)
      return "";
    return std::string(start, end - start);
  };
  std::string raw_(addr p, addr q) final { return translate(p, q); };
  addr tokens_() final {
    if (token_start_ == maxfinity)
      return 0;
    return token_end_ - token_start_ + 1;
  };
  bool range_(addr *p, addr *q) final {
    if (token_start_ == maxfinity) {
      *p = maxfinity;
      *q = maxfinity;
      return false;
    }
    *p = token_start_;
    *q = token_end_;
    return true;
  }

  bool load(addr blob_offset, addr blob_length, std::string *error) {
    const std::string magic = hazel_txt_magic;
    addr header_length = magic.size() + 5 * sizeof(addr);
    if (blob_length < header_length) {
      safe_error(error) = "Hazel txt blob is too short";
      return false;
    }
    std::unique_ptr<char[]> header =
        read_gate_->read(blob_offset, header_length, error);
    if (header == nullptr)
      return false;
    if (std::string(header.get(), magic.size()) != magic) {
      safe_error(error) = "Hazel got bad txt blob magic";
      return false;
    }
    const char *data = header.get() + magic.size();
    addr directory_offset = read_pod<addr>(data);
    data += sizeof(addr);
    addr directory_length = read_pod<addr>(data);
    data += sizeof(addr);
    addr directory_count = read_pod<addr>(data);
    data += sizeof(addr);
    raw_text_length_ = read_pod<addr>(data);
    data += sizeof(addr);
    target_chunk_size_ = read_pod<addr>(data);
    chunk_space_start_ = blob_offset + header_length;
    addr chunk_space_length = blob_length - header_length;
    if (directory_offset < 0 || directory_length < 0 || directory_count < 0 ||
        raw_text_length_ < 0 || target_chunk_size_ <= 0 ||
        directory_length != directory_count * (addr)(2 * sizeof(addr)) ||
        directory_offset > chunk_space_length ||
        directory_length > chunk_space_length - directory_offset) {
      safe_error(error) = "Hazel got bad txt directory";
      return false;
    }
    if (directory_count == 0) {
      if (raw_text_length_ != 0 || directory_length != 0 ||
          directory_offset != 0) {
        safe_error(error) = "Hazel got bad empty txt directory";
        return false;
      }
      estimated_size_ = header_length;
      return true;
    }

    std::unique_ptr<char[]> directory = read_gate_->read(
        chunk_space_start_ + directory_offset, directory_length, error);
    if (directory == nullptr)
      return false;
    map_.reserve(directory_count);
    data = directory.get();
    for (addr i = 0; i < directory_count; i++) {
      HazelTextEntry entry;
      entry.raw_byte_end = read_pod<addr>(data);
      data += sizeof(addr);
      entry.compressed_byte_end = read_pod<addr>(data);
      data += sizeof(addr);
      if (entry.raw_byte_end < 0 || entry.compressed_byte_end < 0 ||
          entry.compressed_byte_end > directory_offset ||
          (!map_.empty() &&
           (entry.raw_byte_end < map_.back().raw_byte_end ||
            entry.compressed_byte_end < map_.back().compressed_byte_end))) {
        safe_error(error) = "Hazel got bad txt chunk boundary";
        return false;
      }
      map_.push_back(entry);
    }
    if (map_.back().raw_byte_end != raw_text_length_ ||
        map_.back().compressed_byte_end != directory_offset) {
      safe_error(error) = "Hazel got bad txt final boundary";
      return false;
    }
    cache_ = std::unique_ptr<HazelTextCacheEntry[]>(
        new HazelTextCacheEntry[map_.size()]);
    estimated_size_ = header_length + map_.back().compressed_byte_end +
                      (addr)map_.size() * 2 * sizeof(addr);
    return true;
  }

  void load_token_range() {
    token_start_ = maxfinity;
    token_end_ = maxfinity;
    addr p, q, value;
    {
      std::lock_guard<std::mutex> lock(hopper_lock_);
      hopper_->tau(minfinity + 1, &p, &q, &value);
      if (p == maxfinity)
        return;
      token_start_ = p;
      hopper_->uat(maxfinity - 1, &p, &q, &value);
      token_end_ = q;
    }
    if (token_end_ < token_start_) {
      token_start_ = maxfinity;
      token_end_ = maxfinity;
    }
  }

  size_t chunk_containing(addr raw_byte) {
    if (raw_byte == raw_text_length_ && !map_.empty())
      return map_.size() - 1;
    auto it = std::upper_bound(map_.begin(), map_.end(), raw_byte,
                               [](addr raw_byte, const HazelTextEntry &entry) {
                                 return raw_byte < entry.raw_byte_end;
                               });
    return it - map_.begin();
  }

  char *obtain(size_t k) {
    HazelTextCacheEntry &entry = cache_[k];
    if (entry.present.load(std::memory_order_acquire))
      return entry.bytes.get();

    addr raw_byte_start = k == 0 ? 0 : map_[k - 1].raw_byte_end;
    addr raw_byte_end = map_[k].raw_byte_end;
    addr compressed_byte_start = k == 0 ? 0 : map_[k - 1].compressed_byte_end;
    addr compressed_byte_end = map_[k].compressed_byte_end;
    addr raw_length = raw_byte_end - raw_byte_start;
    addr compressed_length = compressed_byte_end - compressed_byte_start;
    if (raw_length < 0 || compressed_length < 0)
      return nullptr;
    std::unique_ptr<char[]> compressed = read_gate_->read(
        chunk_space_start_ + compressed_byte_start, compressed_length);
    if (compressed == nullptr)
      return nullptr;
    std::unique_ptr<char[]> raw(new char[raw_length == 0 ? 1 : raw_length]);
    size_t actual = compressor_->tang(compressed.get(), compressed_length,
                                      raw.get(), raw_length);
    if (actual != (size_t)raw_length)
      return nullptr;

    if (entry.present.load(std::memory_order_acquire))
      return entry.bytes.get();
    std::lock_guard<std::mutex> lock(cache_write_lock_);
    if (!entry.present.load(std::memory_order_acquire)) {
      entry.bytes = std::move(raw);
      entry.present.store(true, std::memory_order_release);
    }
    return entry.bytes.get();
  }

  std::string raw_bytes(addr byte_start, addr byte_end) {
    if (byte_start < 0 || byte_end < byte_start ||
        byte_end > raw_text_length_ || byte_start == byte_end)
      return byte_start == byte_end ? std::string() : std::string();
    if (map_.empty())
      return "";
    size_t first = chunk_containing(byte_start);
    size_t last = chunk_containing(byte_end - 1);
    if (first >= map_.size() || last >= map_.size() || first > last)
      return "";
    std::string result;
    result.reserve(byte_end - byte_start);
    for (size_t k = first; k <= last; k++) {
      char *bytes = obtain(k);
      if (bytes == nullptr)
        return "";
      addr chunk_start = k == 0 ? 0 : map_[k - 1].raw_byte_end;
      addr chunk_end = map_[k].raw_byte_end;
      addr start = std::max(byte_start, chunk_start);
      addr end = std::min(byte_end, chunk_end);
      if (end > start)
        result.append(bytes + (start - chunk_start), end - start);
    }
    return result;
  }

  std::string therecipe_;
  std::shared_ptr<ReadGate> read_gate_;
  addr chunk_space_start_;
  addr raw_text_length_;
  addr target_chunk_size_;
  addr estimated_size_ = 0;
  std::vector<HazelTextEntry> map_;
  std::unique_ptr<HazelTextCacheEntry[]> cache_;
  std::shared_ptr<Tokenizer> tokenizer_;
  std::shared_ptr<Compressor> compressor_;
  std::unique_ptr<Hopper> hopper_;
  std::mutex hopper_lock_;
  std::mutex cache_write_lock_;
  addr token_start_ = maxfinity;
  addr token_end_ = maxfinity;
};

bool HazelTxt::merge(const std::vector<std::shared_ptr<HazelTxt>> &txts,
                     std::ostream *out, std::string *error) {
  if (txts.size() < 2) {
    safe_error(error) = "HazelTxt merge needs at least two texts";
    return false;
  }
  if (out == nullptr) {
    safe_error(error) = "HazelTxt merge got no output stream";
    return false;
  }
  for (auto &txt : txts) {
    if (txt == nullptr || txt->read_gate_ == nullptr) {
      safe_error(error) = "HazelTxt merge got null text";
      return false;
    }
    if (txt->target_chunk_size_ != txts[0]->target_chunk_size_) {
      safe_error(error) = "HazelTxt merge got incompatible chunk sizes";
      return false;
    }
  }

  addr target_chunk_size = txts[0]->target_chunk_size_;
  addr blob_start = (addr)out->tellp();
  out->write(hazel_txt_magic.data(), hazel_txt_magic.size());
  write_pod<addr>(out, 0);
  write_pod<addr>(out, 0);
  write_pod<addr>(out, 0);
  write_pod<addr>(out, 0);
  write_pod(out, target_chunk_size);
  addr chunk_space_start = (addr)out->tellp();
  if (out->fail()) {
    safe_error(error) = "Hazel merge failed to write txt blob";
    return false;
  }

  std::vector<HazelTextEntry> directory;
  addr raw_base = 0;
  for (auto &txt : txts) {
    addr previous_raw = 0;
    addr previous_compressed = 0;
    for (auto &entry : txt->map_) {
      addr raw_length = entry.raw_byte_end - previous_raw;
      addr compressed_length = entry.compressed_byte_end - previous_compressed;
      if (raw_length < 0 || compressed_length < 0) {
        safe_error(error) = "Hazel got bad txt chunk boundary";
        return false;
      }
      std::unique_ptr<char[]> compressed =
          txt->read_gate_->read(txt->chunk_space_start_ + previous_compressed,
                                compressed_length, error);
      if (compressed == nullptr)
        return false;
      out->write(compressed.get(), compressed_length);
      if (out->fail()) {
        safe_error(error) = "Hazel merge failed to copy txt chunk";
        return false;
      }
      HazelTextEntry out_entry;
      out_entry.raw_byte_end = raw_base + entry.raw_byte_end;
      out_entry.compressed_byte_end = (addr)out->tellp() - chunk_space_start;
      directory.push_back(out_entry);
      previous_raw = entry.raw_byte_end;
      previous_compressed = entry.compressed_byte_end;
    }
    raw_base += txt->raw_text_length_;
  }

  addr directory_offset = (addr)out->tellp() - chunk_space_start;
  addr directory_count = directory.size();
  for (auto &entry : directory) {
    write_pod(out, entry.raw_byte_end);
    write_pod(out, entry.compressed_byte_end);
  }
  addr directory_length =
      (addr)out->tellp() - (chunk_space_start + directory_offset);
  addr blob_length = (addr)out->tellp() - blob_start;
  out->seekp(blob_start + hazel_txt_magic.size());
  write_pod(out, directory_offset);
  write_pod(out, directory_length);
  write_pod(out, directory_count);
  write_pod(out, raw_base);
  write_pod(out, target_chunk_size);
  out->seekp(blob_start + blob_length);
  if (out->fail()) {
    safe_error(error) = "Hazel merge failed to write txt blob";
    return false;
  }
  return true;
}

bool parse_hazel_name(const std::string &name, addr *sequence_start,
                      addr *sequence_end) {
  const std::string prefix = "hazel.";
  if (name.compare(0, prefix.size(), prefix) != 0)
    return false;
  size_t dot = name.find('.', prefix.size());
  if (dot == std::string::npos)
    return false;
  std::string start = name.substr(prefix.size(), dot - prefix.size());
  std::string end = name.substr(dot + 1);
  if (start.empty() || end.empty())
    return false;
  for (char c : start)
    if (c < '0' || c > '9')
      return false;
  for (char c : end)
    if (c < '0' || c > '9')
      return false;
  try {
    *sequence_start = std::stoll(start);
    *sequence_end = std::stoll(end);
  } catch (...) {
    return false;
  }
  return true;
}

struct HazelMergeOutput {
  std::string filename;
  std::fstream out;
  std::vector<HazelBlob> blobs = {{"idx", 0, 0}, {"txt", 0, 0}};
  addr dictionary_offset = 0;

  bool open(const std::string &tempname, const std::string &dna,
            std::string *error) {
    filename = tempname;
    out.open(filename, std::ios::binary | std::ios::out);
    if (out.fail()) {
      safe_error(error) = "Hazel merge can't create shard: " + filename;
      return false;
    }
    const std::string file_header = cottontail_file_magic;
    std::string dictionary = hazel_blob_dictionary(blobs);
    out.write(file_header.data(), file_header.size());
    out.write(dna.data(), dna.size());
    out.put('\n');
    dictionary_offset = out.tellp();
    out.write(dictionary.data(), dictionary.size());
    if (out.fail()) {
      safe_error(error) = "Hazel merge failed to write shard header";
      return false;
    }
    return true;
  }

  bool patch_dictionary(std::string *error) {
    addr end = out.tellp();
    std::string dictionary = hazel_blob_dictionary(blobs);
    out.seekp(dictionary_offset);
    out.write(dictionary.data(), dictionary.size());
    out.seekp(end);
    if (out.fail()) {
      safe_error(error) = "Hazel merge failed to write blob dictionary";
      return false;
    }
    return true;
  }

  bool close(std::string *error) {
    if (!patch_dictionary(error))
      return false;
    out.close();
    if (out.fail()) {
      safe_error(error) = "Hazel merge failed to close shard";
      return false;
    }
    return true;
  }
};

std::string hazel_sidecar_name(const std::string &dst,
                               const std::string &prefix) {
  std::filesystem::path target(dst);
  std::filesystem::path directory = target.parent_path();
  std::filesystem::path sidecar = prefix + "." + target.filename().string();
  if (directory.empty())
    return sidecar.string();
  return (directory / sidecar).string();
}

bool hazel_path_exists(const std::string &filename, bool *exists,
                       std::string *error) {
  std::error_code ec;
  *exists = std::filesystem::exists(filename, ec);
  if (ec) {
    safe_error(error) =
        "Hazel merge can't inspect file: " + filename + ": " + ec.message();
    return false;
  }
  return true;
}

bool hazel_remove_if_exists(const std::string &filename, std::string *error) {
  bool exists;
  if (!hazel_path_exists(filename, &exists, error))
    return false;
  if (!exists)
    return true;
  std::error_code ec;
  std::filesystem::remove(filename, ec);
  if (ec) {
    safe_error(error) =
        "Hazel merge can't remove file: " + filename + ": " + ec.message();
    return false;
  }
  return true;
}

bool hazel_cleanup_prefix_files(const std::string &prefix, std::string *error) {
  std::filesystem::path target(prefix);
  std::filesystem::path directory = target.parent_path();
  if (directory.empty())
    directory = ".";
  std::string base = target.filename().string() + ".";

  std::error_code ec;
  bool exists = std::filesystem::exists(directory, ec);
  if (ec) {
    safe_error(error) =
        "Hazel merge can't inspect directory: " + directory.string() + ": " +
        ec.message();
    return false;
  }
  if (!exists)
    return true;

  std::filesystem::directory_iterator it(directory, ec);
  std::filesystem::directory_iterator end;
  if (ec) {
    safe_error(error) =
        "Hazel merge can't list directory: " + directory.string() + ": " +
        ec.message();
    return false;
  }
  for (; it != end; it.increment(ec)) {
    if (ec) {
      safe_error(error) =
          "Hazel merge can't list directory: " + directory.string() + ": " +
          ec.message();
      return false;
    }
    std::string name = it->path().filename().string();
    if (name.compare(0, base.size(), base) != 0)
      continue;
    std::error_code remove_error;
    std::filesystem::remove(it->path(), remove_error);
    if (remove_error) {
      safe_error(error) =
          "Hazel merge can't remove file: " + it->path().string() + ": " +
          remove_error.message();
      return false;
    }
  }
  return true;
}

bool hazel_cleanup_merge_files(const std::string &dst, std::string *error) {
  for (const char *prefix : {"mrg", "pst", "dct"})
    if (!hazel_remove_if_exists(hazel_sidecar_name(dst, prefix), error))
      return false;
  return hazel_cleanup_prefix_files(dst, error);
}

bool hazel_cleanup_published_merge(const std::string &dst, addr sequence_start,
                                   addr sequence_end, std::string *error) {
  return hazel_cleanup_merge_files(dst, error) &&
         hazel_remove_merge_segments(dst, sequence_start, sequence_end, error);
}

bool has_suffix(const std::string &name, const std::string &suffix) {
  return name.size() >= suffix.size() &&
         name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool parse_old_hazel_sidecar(const std::string &name, OwslaShard *target) {
  for (const char *suffix : {".tmp", ".pst", ".dct"}) {
    if (!has_suffix(name, suffix))
      continue;
    std::string base = name.substr(0, name.size() - std::strlen(suffix));
    return owsla_parse_shard_name(base, "hazel", target);
  }
  return false;
}

bool normalize_hazel_shards(std::vector<OwslaShard> *found,
                            std::vector<OwslaShard> *living,
                            std::vector<OwslaShard> *dead, std::string *error) {
  std::sort(found->begin(), found->end(),
            [](const auto &a, const auto &b) -> bool {
              return a.start < b.start || (a.start == b.start && a.end > b.end);
            });
  living->clear();
  dead->clear();
  for (auto &shard : *found) {
    if (living->empty() || living->back().end < shard.start) {
      living->push_back(shard);
    } else if (living->back().end >= shard.end) {
      dead->push_back(shard);
    } else {
      safe_error(error) = "Filename sequence error for hazel: " + shard.name;
      return false;
    }
  }
  return true;
}

bool shadowed_by_living_hazel(const OwslaShard &dead,
                              const std::vector<OwslaShard> &living) {
  for (auto &live : living)
    if (owsla_range_contains(live, dead))
      return true;
  return false;
}

bool verify_dead_hazels(const std::vector<OwslaShard> &living,
                        const std::vector<OwslaShard> &dead,
                        std::string *error) {
  for (auto &shard : dead)
    if (!shadowed_by_living_hazel(shard, living)) {
      safe_error(error) = "Unshadowed dead hazel shard: " + shard.name;
      return false;
    }
  return true;
}

bool same_range(const OwslaShard &shard, addr start, addr end) {
  return shard.start == start && shard.end == end;
}

bool has_hazel(const std::vector<OwslaShard> &hazels, addr start, addr end) {
  for (auto &hazel : hazels)
    if (same_range(hazel, start, end))
      return true;
  return false;
}

bool hazel_source_group(const std::vector<OwslaShard> &hazels, addr start,
                        addr end, std::vector<OwslaShard> *sources) {
  if (sources != nullptr)
    sources->clear();
  for (size_t i = 0; i < hazels.size(); i++) {
    if (hazels[i].start != start)
      continue;
    std::vector<OwslaShard> group;
    for (size_t j = i; j < hazels.size(); j++) {
      if (hazels[j].end > end)
        break;
      group.push_back(hazels[j]);
      if (hazels[j].end == end) {
        if (group.size() < 2)
          return false;
        if (sources != nullptr)
          *sources = group;
        return true;
      }
    }
  }
  return false;
}

bool remove_working_names(std::shared_ptr<Working> working,
                          const std::vector<std::string> &names,
                          std::string *error) {
  for (auto &name : names)
    if (!working->remove(name, error))
      return false;
  return true;
}

bool normalize_dna_for_activated_hazel_merge(
    const std::map<std::string, std::string> &input, std::string *normalized,
    std::string *error) {
  std::map<std::string, std::string> parameters = input;
  parameters.erase("parameters");
  auto hazel = parameters.find("hazel");
  if (hazel != parameters.end()) {
    std::map<std::string, std::string> metadata;
    if (!cook(hazel->second, &metadata, error))
      return false;
    metadata.erase("sequence_start");
    metadata.erase("sequence_end");
    hazel->second = freeze(metadata);
  }
  *normalized = freeze(parameters);
  return true;
}

bool hazel_sequence_range(const std::map<std::string, std::string> &parameters,
                          bool *present, addr *sequence_start,
                          addr *sequence_end, std::string *error) {
  *present = false;
  auto hazel = parameters.find("hazel");
  if (hazel == parameters.end())
    return true;
  std::map<std::string, std::string> metadata;
  if (!cook(hazel->second, &metadata, error))
    return false;
  auto start = metadata.find("sequence_start");
  auto end = metadata.find("sequence_end");
  if (start == metadata.end() && end == metadata.end())
    return true;
  if (start == metadata.end() || end == metadata.end()) {
    safe_error(error) = "Hazel DNA has incomplete sequence range";
    return false;
  }
  try {
    *sequence_start = std::stoll(start->second);
    *sequence_end = std::stoll(end->second);
  } catch (...) {
    safe_error(error) = "Hazel DNA has bad sequence range";
    return false;
  }
  if (*sequence_start < 0 || *sequence_end < *sequence_start) {
    safe_error(error) = "Hazel DNA has bad sequence range";
    return false;
  }
  *present = true;
  return true;
}

bool merged_activated_hazel_dna(
    const std::map<std::string, std::string> &first,
    const std::map<std::string, std::string> &last, bool sequence_present,
    addr sequence_start, addr sequence_end,
    std::shared_ptr<std::map<std::string, std::string>> parameters,
    std::string *dna, std::string *error) {
  std::map<std::string, std::string> output = first;
  if (sequence_present) {
    auto hazel = output.find("hazel");
    if (hazel == output.end()) {
      safe_error(error) = "Hazel DNA has no hazel metadata";
      return false;
    }
    std::map<std::string, std::string> metadata;
    if (!cook(hazel->second, &metadata, error))
      return false;
    metadata["sequence_start"] = std::to_string(sequence_start);
    metadata["sequence_end"] = std::to_string(sequence_end);
    hazel->second = freeze(metadata);
  }
  if (parameters != nullptr) {
    output["parameters"] = freeze(*parameters);
  } else {
    auto inherited = last.find("parameters");
    if (inherited == last.end())
      output.erase("parameters");
    else
      output["parameters"] = inherited->second;
  }
  *dna = freeze(output);
  return true;
}

std::shared_ptr<Hazel> activate_hazel(const std::string &filename,
                                      std::string *error) {
  std::shared_ptr<Warren> warren = Warren::make(filename, error);
  if (warren == nullptr)
    return nullptr;
  std::shared_ptr<Hazel> hazel = std::dynamic_pointer_cast<Hazel>(warren);
  if (hazel == nullptr) {
    safe_error(error) = "Hazel merge got non-Hazel output: " + filename;
    return nullptr;
  }
  return hazel;
}

} // namespace

bool Hazel::merge(std::shared_ptr<Working> working,
                  const std::vector<std::string> &hazels,
                  const std::string &parameters, std::string *error) {
  (void)parameters;
  if (working == nullptr) {
    safe_error(error) = "Hazel merge needs a working directory";
    return false;
  }
  if (hazels.size() < 2) {
    safe_error(error) = "Hazel merge needs at least two shards";
    return false;
  }

  std::vector<std::shared_ptr<Hazel>> activated;
  activated.reserve(hazels.size());
  addr first_start = 0;
  addr last_end = 0;
  addr previous_end = 0;
  for (size_t i = 0; i < hazels.size(); i++) {
    addr sequence_start;
    addr sequence_end;
    if (!parse_hazel_name(hazels[i], &sequence_start, &sequence_end)) {
      safe_error(error) = "Hazel merge got bad shard name: " + hazels[i];
      return false;
    }
    if (i == 0) {
      first_start = sequence_start;
    } else if (sequence_start <= previous_end) {
      safe_error(error) = "Hazel merge got unordered shards";
      return false;
    }
    previous_end = sequence_end;
    last_end = sequence_end;

    std::string filename = working->make_name(hazels[i]);
    std::shared_ptr<Warren> warren = Warren::make(filename, error);
    if (warren == nullptr)
      return false;
    std::shared_ptr<Hazel> hazel = std::dynamic_pointer_cast<Hazel>(warren);
    if (hazel == nullptr) {
      safe_error(error) = "Hazel merge got non-Hazel shard: " + hazels[i];
      return false;
    }

    bool dna_sequence_present;
    addr dna_sequence_start;
    addr dna_sequence_end;
    if (!hazel_sequence_range(hazel->parameters_, &dna_sequence_present,
                              &dna_sequence_start, &dna_sequence_end, error))
      return false;
    if (!dna_sequence_present) {
      safe_error(error) = "Hazel merge needs sequence metadata";
      return false;
    }
    if (dna_sequence_start != sequence_start ||
        dna_sequence_end != sequence_end) {
      safe_error(error) = "Hazel filename and DNA sequence ranges differ";
      return false;
    }
    activated.push_back(hazel);
  }

  std::string final_name = hazel_default_name(first_start, last_end);
  std::string final_filename = working->make_name(final_name);
  return Hazel::merge(activated, final_filename, nullptr, error) != nullptr;
}

bool Hazel::sanitize(std::shared_ptr<Working> working,
                     std::vector<OwslaShard> *hazels,
                     std::vector<HazelMergeRecovery> *recoveries,
                     std::string *error) {
  if (hazels != nullptr)
    hazels->clear();
  if (recoveries != nullptr)
    recoveries->clear();
  if (working == nullptr)
    return true;

  std::vector<OwslaShard> found;
  std::vector<std::string> old_sidecars;
  for (auto &name : working->ls("hazel")) {
    OwslaShard shard;
    if (owsla_parse_shard_name(name, "hazel", &shard)) {
      found.push_back(shard);
      continue;
    }
    if (parse_old_hazel_sidecar(name, &shard)) {
      old_sidecars.push_back(name);
      continue;
    }
    safe_error(error) = "Filename format error for hazel: " + name;
    return false;
  }

  std::vector<OwslaShard> living;
  std::vector<OwslaShard> dead;
  if (!normalize_hazel_shards(&found, &living, &dead, error) ||
      !verify_dead_hazels(living, dead, error))
    return false;

  for (const char *prefix : {"mrg", "pst", "dct"}) {
    std::vector<std::string> legacy;
    std::string full_prefix = std::string(prefix) + ".";
    for (auto &name : working->ls(prefix))
      if (name.compare(0, full_prefix.size(), full_prefix) == 0)
        legacy.push_back(name);
    if (!remove_working_names(working, legacy, error))
      return false;
  }

  std::map<std::pair<addr, addr>, std::vector<HazelMergeSegmentName>> groups;
  std::vector<std::string> invalid_names;
  for (auto &name : working->ls("merge")) {
    if (name.compare(0, 6, "merge.") != 0) {
      invalid_names.push_back(name);
      continue;
    }
    HazelMergeSegmentName segment;
    if (!parse_hazel_merge_segment_name(name, &segment)) {
      invalid_names.push_back(name);
      continue;
    }
    groups[{segment.target.start, segment.target.end}].push_back(segment);
  }
  if (!remove_working_names(working, invalid_names, error))
    return false;

  std::vector<HazelMergeRecovery> candidates;
  for (auto &item : groups) {
    auto &segments = item.second;
    bool normalized = normalize_hazel_merge_segment_names(&segments);
    std::vector<std::string> names;
    names.reserve(segments.size());
    for (auto &segment : segments)
      names.push_back(segment.name);
    if (!normalized) {
      if (!remove_working_names(working, names, error))
        return false;
      continue;
    }
    addr start = segments.front().target.start;
    addr end = segments.front().target.end;
    if (has_hazel(living, start, end)) {
      if (!remove_working_names(working, names, error))
        return false;
      continue;
    }
    std::vector<OwslaShard> sources;
    if (!hazel_source_group(living, start, end, &sources)) {
      if (!remove_working_names(working, names, error))
        return false;
      continue;
    }
    HazelMergeRecovery recovery;
    recovery.target = OwslaShard(start, end, hazel_default_name(start, end));
    recovery.sources = sources;
    recovery.segment_count = segments.size();
    candidates.push_back(recovery);
  }

  for (auto &shard : dead)
    if (!working->remove(shard.name, error))
      return false;
  if (!remove_working_names(working, old_sidecars, error))
    return false;
  std::sort(candidates.begin(), candidates.end());

  if (hazels != nullptr)
    *hazels = living;
  if (recoveries != nullptr)
    *recoveries = candidates;
  return true;
}

std::shared_ptr<Hazel>
Hazel::merge(const std::vector<std::shared_ptr<Hazel>> &hazels,
             const std::string &dst,
             std::shared_ptr<std::map<std::string, std::string>> parameters,
             std::string *error) {
  if (hazels.size() < 2) {
    safe_error(error) = "Hazel merge needs at least two shards";
    return nullptr;
  }
  if (dst == "") {
    safe_error(error) = "Hazel merge got empty destination";
    return nullptr;
  }

  std::vector<std::shared_ptr<HazelIdx>> idxs;
  std::vector<std::shared_ptr<HazelTxt>> txts;
  std::vector<addr> text_lengths;
  idxs.reserve(hazels.size());
  txts.reserve(hazels.size());
  text_lengths.reserve(hazels.size());

  std::string normalized;
  addr sequence_start = 0;
  addr sequence_end = 0;
  addr previous_sequence_end = 0;
  for (size_t i = 0; i < hazels.size(); i++) {
    auto hazel = hazels[i];
    if (hazel == nullptr) {
      safe_error(error) = "Hazel merge got null shard";
      return nullptr;
    }
    if (hazel->featurizer_ == nullptr || hazel->idx_ == nullptr ||
        hazel->txt_ == nullptr) {
      safe_error(error) = "Hazel merge got incomplete shard";
      return nullptr;
    }

    auto idx = std::dynamic_pointer_cast<HazelIdx>(hazel->idx_);
    if (idx == nullptr) {
      safe_error(error) = "Hazel merge got non-Hazel idx";
      return nullptr;
    }
    auto txt = std::dynamic_pointer_cast<HazelTxt>(hazel->txt_);
    if (txt == nullptr) {
      safe_error(error) = "Hazel merge got non-Hazel txt";
      return nullptr;
    }

    std::string current;
    if (!normalize_dna_for_activated_hazel_merge(hazel->parameters_, &current,
                                                 error))
      return nullptr;
    if (i == 0)
      normalized = current;
    else if (current != normalized) {
      safe_error(error) = "Hazel merge got incompatible DNA";
      return nullptr;
    }

    bool current_sequence_present;
    addr current_sequence_start;
    addr current_sequence_end;
    if (!hazel_sequence_range(hazel->parameters_, &current_sequence_present,
                              &current_sequence_start, &current_sequence_end,
                              error))
      return nullptr;
    if (!current_sequence_present) {
      safe_error(error) = "Hazel merge needs sequence metadata";
      return nullptr;
    }
    if (i == 0) {
      sequence_start = current_sequence_start;
      sequence_end = current_sequence_end;
      previous_sequence_end = current_sequence_end;
    } else {
      if (current_sequence_start <= previous_sequence_end) {
        safe_error(error) = "Hazel merge got unordered shards";
        return nullptr;
      }
      sequence_end = current_sequence_end;
      previous_sequence_end = current_sequence_end;
    }

    idxs.push_back(idx);
    txts.push_back(txt);
    text_lengths.push_back(txt->raw_text_length());
  }

  std::string dna;
  if (!merged_activated_hazel_dna(
          hazels.front()->parameters_, hazels.back()->parameters_, true,
          sequence_start, sequence_end, parameters, &dna, error))
    return nullptr;

  bool exists;
  if (!hazel_path_exists(dst, &exists, error))
    return nullptr;
  if (exists) {
    if (!hazel_cleanup_published_merge(dst, sequence_start, sequence_end,
                                       error))
      return nullptr;
    return activate_hazel(dst, error);
  }
  if (!hazel_cleanup_merge_files(dst, error))
    return nullptr;

  addr text_chunk_feature =
      hazels.front()->featurizer_->featurize(text_chunk_tag);
  if (!HazelIdx::prepare_merge(idxs, text_lengths, text_chunk_feature, dst,
                               sequence_start, sequence_end, error))
    return nullptr;

  std::string tempname = dst + ".tmp";
  HazelMergeOutput output;
  auto remove_temp = [&]() {
    output.out.close();
    hazel_remove_if_exists(tempname, nullptr);
  };

  if (!output.open(tempname, dna, error)) {
    remove_temp();
    return nullptr;
  }

  output.blobs[0].offset = (addr)output.out.tellp();
  if (!HazelIdx::write_merge(idxs, dst, sequence_start, sequence_end,
                             &output.out, error)) {
    remove_temp();
    return nullptr;
  }
  output.blobs[0].length = (addr)output.out.tellp() - output.blobs[0].offset;
  if (output.out.fail() || output.blobs[0].length < 0) {
    safe_error(error) = "Hazel merge failed to write idx blob";
    remove_temp();
    return nullptr;
  }

  output.blobs[1].offset = (addr)output.out.tellp();
  if (!HazelTxt::merge(txts, &output.out, error)) {
    remove_temp();
    return nullptr;
  }
  output.blobs[1].length = (addr)output.out.tellp() - output.blobs[1].offset;
  if (output.out.fail() || output.blobs[1].length < 0) {
    safe_error(error) = "Hazel merge failed to write txt blob";
    remove_temp();
    return nullptr;
  }

  if (!output.close(error)) {
    remove_temp();
    return nullptr;
  }

  if (link(tempname.c_str(), dst.c_str()) != 0) {
    if (!hazel_path_exists(dst, &exists, error)) {
      remove_temp();
      return nullptr;
    }
    if (exists) {
      if (!hazel_cleanup_published_merge(dst, sequence_start, sequence_end,
                                         error)) {
        remove_temp();
        return nullptr;
      }
      return activate_hazel(dst, error);
    }
    safe_error(error) = "Hazel merge can't link shard: " + dst;
    remove_temp();
    return nullptr;
  }

  if (!hazel_cleanup_published_merge(dst, sequence_start, sequence_end, error))
    return nullptr;
  return activate_hazel(dst, error);
}

std::shared_ptr<Warren> Hazel::make(const std::string &filename,
                                    const std::string &dna,
                                    std::string *error) {
  std::map<std::string, std::string> parameters;
  if (!cook(dna, &parameters, error))
    return nullptr;
  auto warren = parameters.find("warren");
  if (warren == parameters.end() || warren->second != "hazel") {
    safe_error(error) = "Hazel got non-Hazel DNA";
    return nullptr;
  }

  std::string featurizer_name, featurizer_recipe;
  std::string tokenizer_name, tokenizer_recipe;
  std::string idx_name, idx_recipe;
  std::string txt_name, txt_recipe;
  if (!name_and_recipe(parameters, "featurizer", &featurizer_name,
                       &featurizer_recipe, error) ||
      !name_and_recipe(parameters, "tokenizer", &tokenizer_name,
                       &tokenizer_recipe, error) ||
      !name_and_recipe(parameters, "idx", &idx_name, &idx_recipe, error) ||
      !name_and_recipe(parameters, "txt", &txt_name, &txt_recipe, error))
    return nullptr;
  if (idx_name != "hazel") {
    safe_error(error) = "Hazel DNA has non-Hazel idx: " + idx_name;
    return nullptr;
  }
  if (txt_name != "hazel") {
    safe_error(error) = "Hazel DNA has non-Hazel txt: " + txt_name;
    return nullptr;
  }
  bool sequence_present;
  addr sequence_start = -1;
  addr sequence_end = -1;
  if (!hazel_sequence_range(parameters, &sequence_present, &sequence_start,
                            &sequence_end, error))
    return nullptr;

  std::shared_ptr<Featurizer> featurizer =
      Featurizer::make(featurizer_name, featurizer_recipe, error);
  if (featurizer == nullptr)
    return nullptr;
  std::shared_ptr<Tokenizer> tokenizer =
      Tokenizer::make(tokenizer_name, tokenizer_recipe, error);
  if (tokenizer == nullptr)
    return nullptr;
  std::map<std::string, HazelBlob> blobs;
  if (!read_blob_dictionary(filename, &blobs, error))
    return nullptr;
  auto idx_blob = blobs.find("idx");
  auto txt_blob = blobs.find("txt");
  if (idx_blob == blobs.end() || txt_blob == blobs.end()) {
    safe_error(error) = "Hazel missing idx or txt blob";
    return nullptr;
  }
  std::shared_ptr<HazelFile> file = HazelFile::make(filename, error);
  if (file == nullptr)
    return nullptr;
  std::shared_ptr<HazelIdx> hazel_idx =
      HazelIdx::make(idx_recipe, filename, file, idx_blob->second, error);
  if (hazel_idx == nullptr)
    return nullptr;
  std::unique_ptr<Hopper> text_chunk_hopper =
      hazel_idx->hopper(featurizer->featurize(text_chunk_tag));
  if (text_chunk_hopper == nullptr) {
    safe_error(error) = "Hazel can't make text chunk hopper";
    return nullptr;
  }
  std::shared_ptr<HazelTxt> hazel_txt = HazelTxt::make(
      txt_recipe, filename, txt_blob->second.offset, txt_blob->second.length,
      tokenizer, std::move(text_chunk_hopper), error);
  if (hazel_txt == nullptr)
    return nullptr;
  std::shared_ptr<Txt> txt = Txt::wrap(txt_recipe, hazel_txt, error);
  if (txt == nullptr)
    return nullptr;

  std::shared_ptr<Hazel> hazel =
      std::shared_ptr<Hazel>(new Hazel(featurizer, tokenizer, hazel_idx, txt));
  hazel->name_ = "hazel";
  hazel->filename_ = filename;
  hazel->dna_ = dna;
  hazel->parameters_ = parameters;
  hazel->sequence_start_ = sequence_start;
  hazel->sequence_end_ = sequence_end;
  hazel->estimated_size_ =
      hazel_idx->estimated_size() + hazel_txt->estimated_size();
  hazel->annotator_ = NullAnnotator::make("", error);
  if (hazel->annotator_ == nullptr)
    return nullptr;
  hazel->appender_ = NullAppender::make("", error);
  if (hazel->appender_ == nullptr)
    return nullptr;

  auto extra = parameters.find("parameters");
  if (extra != parameters.end()) {
    std::map<std::string, std::string> extra_parameters;
    if (!cook(extra->second, &extra_parameters, error))
      return nullptr;
    auto container = extra_parameters.find("container");
    if (container != extra_parameters.end())
      hazel->default_container_ = container->second;
    auto stemmer = extra_parameters.find("stemmer");
    if (stemmer != extra_parameters.end() && stemmer->second != "") {
      hazel->stemmer_ = Stemmer::make(stemmer->second, "", error);
      if (hazel->stemmer_ == nullptr)
        return nullptr;
    }
  }
  return hazel;
}

std::shared_ptr<SimplePosting> Hazel::posting(addr feature) {
  std::shared_ptr<HazelIdx> idx =
      std::static_pointer_cast<HazelIdx>(this->idx());
  return idx->posting(feature);
}

void Hazel::get_sequence(addr *start, addr *end) const {
  *start = sequence_start_;
  *end = sequence_end_;
}

bool Hazel::discard(std::string *error) {
  (void)error;
  if (filename_ != "")
    std::remove(filename_.c_str());
  return true;
}

std::shared_ptr<Warren> Hazel::clone_(std::string *error) {
  std::shared_ptr<Hazel> hazel =
      std::shared_ptr<Hazel>(new Hazel(featurizer_, tokenizer_, idx_, txt_));
  hazel->name_ = name_;
  hazel->filename_ = filename_;
  hazel->dna_ = dna_;
  hazel->parameters_ = parameters_;
  hazel->estimated_size_ = estimated_size_;
  hazel->sequence_start_ = sequence_start_;
  hazel->sequence_end_ = sequence_end_;
  hazel->default_container_ = default_container_;
  hazel->stemmer_ = stemmer_;
  hazel->annotator_ = annotator_;
  hazel->appender_ = appender_;
  if (started())
    hazel->start();
  return hazel;
}

bool Hazel::set_parameter_(const std::string &key, const std::string &value,
                           std::string *error) {
  safe_error(error) = "Hazel can't set its parameters";
  return false;
}

bool Hazel::get_parameter_(const std::string &key, std::string *value,
                           std::string *error) {
  auto extra = parameters_.find("parameters");
  if (extra == parameters_.end()) {
    *value = "";
    return true;
  }
  std::map<std::string, std::string> extra_parameters;
  if (!cook(extra->second, &extra_parameters, error))
    return false;
  auto item = extra_parameters.find(key);
  if (item == extra_parameters.end())
    *value = "";
  else
    *value = item->second;
  return true;
}

} // namespace cottontail
