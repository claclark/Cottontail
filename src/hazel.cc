#include "src/hazel.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "src/annotator.h"
#include "src/appender.h"
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

const std::string text_chunk_tag = "\035"; // ASCII group separator

template <typename T> T read_pod(const char *data) {
  T value;
  memcpy(&value, data, sizeof(T));
  return value;
}

template <typename T> bool read_pod(std::fstream *in, T *value) {
  in->read(reinterpret_cast<char *>(value), sizeof(T));
  return !in->fail();
}

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

struct HazelBlob {
  std::string name;
  addr offset;
  addr length;
};

bool skip_hazel_dna(std::fstream *in, std::string *error) {
  const std::string magic = "#COTTONTAIL\n";
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
  const std::string magic = "COTTONTAIL_HAZEL_BLOBS_V1\n";
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

struct HazelPostingEntry {
  addr feature;
  addr end;
  addr count_or_p;
};

bool locate_posting(const std::vector<HazelPostingEntry> &directory,
                    addr feature, size_t *index) {
  auto it = std::lower_bound(
      directory.begin(), directory.end(), feature,
      [](const HazelPostingEntry &entry, addr feature) {
        return entry.feature < feature;
      });
  if (it == directory.end() || it->feature != feature)
    return false;
  *index = it - directory.begin();
  return true;
}

class HazelIdx final : public Idx {
public:
  static std::shared_ptr<Idx> make(const std::string &recipe,
                                   std::shared_ptr<HazelFile> file,
                                   const HazelBlob &blob,
                                   std::string *error = nullptr) {
    std::shared_ptr<HazelIdx> idx = std::shared_ptr<HazelIdx>(new HazelIdx());
    idx->therecipe_ = recipe;
    idx->file_ = file;
    idx->blob_offset_ = blob.offset;
    idx->blob_length_ = blob.length;
    if (!compressor_from_recipe(recipe, "posting_compressor",
                                "posting_compressor_recipe",
                                &idx->posting_compressor_, error) ||
        !compressor_from_recipe(recipe, "fvalue_compressor",
                                "fvalue_compressor_recipe",
                                &idx->fvalue_compressor_, error))
      return nullptr;
    idx->posting_factory_ = SimplePostingFactory::make(
        idx->posting_compressor_, idx->fvalue_compressor_);
    if (!idx->load(error))
      return nullptr;
    return idx;
  }

  virtual ~HazelIdx(){};
  HazelIdx(const HazelIdx &) = delete;
  HazelIdx &operator=(const HazelIdx &) = delete;
  HazelIdx(HazelIdx &&) = delete;
  HazelIdx &operator=(HazelIdx &&) = delete;

private:
  HazelIdx(){};
  std::string recipe_() final { return therecipe_; };
  std::unique_ptr<Hopper> hopper_(addr feature) final {
    size_t index;
    if (!locate_posting(directory_, feature, &index))
      return std::make_unique<EmptyHopper>();
    addr start = index == 0 ? postings_start_ : directory_[index - 1].end;
    addr end = directory_[index].end;
    if (start == end)
      return std::make_unique<SingletonHopper>(directory_[index].count_or_p,
                                               directory_[index].count_or_p,
                                               0.0);
    if (start > end) {
      assert(false);
      return std::make_unique<EmptyHopper>();
    }
    std::string bytes(end - start, '\0');
    std::string error;
    if (!file_->read(blob_offset_ + start, &bytes[0], bytes.size(), &error)) {
      assert(false);
      return std::make_unique<EmptyHopper>();
    }
    std::shared_ptr<SimplePosting> posting =
        posting_factory_->posting_from_compressed_blob(bytes.data(),
                                                       bytes.size(), &error);
    assert(posting != nullptr);
    if (posting == nullptr)
      return std::make_unique<EmptyHopper>();
    return posting->hopper();
  };
  addr count_(addr feature) final {
    size_t index;
    if (!locate_posting(directory_, feature, &index))
      return 0;
    addr start = index == 0 ? postings_start_ : directory_[index - 1].end;
    if (start == directory_[index].end)
      return 1;
    return directory_[index].count_or_p;
  };
  addr vocab_() final { return directory_.size(); };

  bool load(std::string *error) {
    const std::string magic = "COTTONTAIL_HAZEL_IDX_V1\n";
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
  std::shared_ptr<Compressor> posting_compressor_;
  std::shared_ptr<Compressor> fvalue_compressor_;
  std::shared_ptr<SimplePostingFactory> posting_factory_;
};

struct HazelTextEntry {
  addr raw_byte_end;
  addr compressed_byte_end;
};

struct HazelTextCacheEntry {
  std::unique_ptr<char[]> bytes;
  std::atomic<bool> present{false};
};

class HazelTxt final : public Txt {
public:
  static std::shared_ptr<Txt> make(const std::string &recipe,
                                   const std::string &filename,
                                   addr blob_offset, addr blob_length,
                                   std::shared_ptr<Tokenizer> tokenizer,
                                   std::unique_ptr<Hopper> hopper,
                                   std::string *error = nullptr) {
    std::shared_ptr<HazelTxt> txt = std::shared_ptr<HazelTxt>(new HazelTxt());
    txt->therecipe_ = recipe;
    txt->read_gate_ = ReadGate::make(filename, error);
    if (txt->read_gate_ == nullptr)
      return nullptr;
    txt->tokenizer_ = tokenizer;
    txt->hopper_ = std::move(hopper);
    if (!compressor_from_recipe(recipe, "compressor", "compressor_recipe",
                                &txt->compressor_, error) ||
        !txt->load(blob_offset, blob_length, error) ||
        !txt->load_token_range())
      return nullptr;
    return txt;
  }

  virtual ~HazelTxt(){};
  HazelTxt(const HazelTxt &) = delete;
  HazelTxt &operator=(const HazelTxt &) = delete;
  HazelTxt(HazelTxt &&) = delete;
  HazelTxt &operator=(HazelTxt &&) = delete;

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
      end = tokenizer_->skip(base + offset, limit - (base + offset),
                             q - p1 + 1);
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
    const std::string magic = "COTTONTAIL_HAZEL_TXT_V1\n";
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
    addr target_chunk_size = read_pod<addr>(data);
    chunk_space_start_ = blob_offset + header_length;
    addr chunk_space_length = blob_length - header_length;
    if (directory_offset < 0 || directory_length < 0 || directory_count < 0 ||
        raw_text_length_ < 0 || target_chunk_size <= 0 ||
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
    return true;
  }

  bool load_token_range() {
    token_start_ = maxfinity;
    token_end_ = maxfinity;
    addr p, q, value;
    {
      std::lock_guard<std::mutex> lock(hopper_lock_);
      hopper_->tau(minfinity + 1, &p, &q, &value);
      if (p == maxfinity)
        return true;
      token_start_ = p;
      hopper_->uat(maxfinity - 1, &p, &q, &value);
      token_end_ = q;
    }
    if (token_end_ < token_start_) {
      token_start_ = maxfinity;
      token_end_ = maxfinity;
    }
    return true;
  }

  size_t chunk_containing(addr raw_byte) {
    if (raw_byte == raw_text_length_ && !map_.empty())
      return map_.size() - 1;
    auto it = std::upper_bound(
        map_.begin(), map_.end(), raw_byte,
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
    std::unique_ptr<char[]> compressed =
        read_gate_->read(chunk_space_start_ + compressed_byte_start,
                         compressed_length);
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

} // namespace

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
  std::shared_ptr<Idx> idx =
      HazelIdx::make(idx_recipe, file, idx_blob->second, error);
  if (idx == nullptr)
    return nullptr;
  std::unique_ptr<Hopper> text_chunk_hopper =
      idx->hopper(featurizer->featurize(text_chunk_tag));
  if (text_chunk_hopper == nullptr) {
    safe_error(error) = "Hazel can't make text chunk hopper";
    return nullptr;
  }
  std::shared_ptr<Txt> txt =
      Txt::wrap(txt_recipe,
                HazelTxt::make(txt_recipe, filename, txt_blob->second.offset,
                               txt_blob->second.length, tokenizer,
                               std::move(text_chunk_hopper), error),
                error);
  if (txt == nullptr)
    return nullptr;

  std::shared_ptr<Hazel> hazel =
      std::shared_ptr<Hazel>(new Hazel(featurizer, tokenizer, idx, txt));
  hazel->name_ = "hazel";
  hazel->filename_ = filename;
  hazel->dna_ = dna;
  hazel->parameters_ = parameters;
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

std::shared_ptr<Warren> Hazel::clone_(std::string *error) {
  std::shared_ptr<Hazel> hazel =
      std::shared_ptr<Hazel>(new Hazel(featurizer_, tokenizer_, idx_, txt_));
  hazel->name_ = name_;
  hazel->filename_ = filename_;
  hazel->dna_ = dna_;
  hazel->parameters_ = parameters_;
  hazel->default_container_ = default_container_;
  hazel->stemmer_ = stemmer_;
  hazel->annotator_ = annotator_;
  hazel->appender_ = appender_;
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
