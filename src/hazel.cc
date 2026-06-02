#include "src/hazel.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <unistd.h>
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

template <typename T> void write_pod(std::ostream *out, T value) {
  out->write(reinterpret_cast<char *>(&value), sizeof(T));
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

void mark_cache_ready(std::shared_ptr<CacheRecord> entry) {
  {
    std::lock_guard<std::mutex> lock(entry->lock);
    entry->ready = true;
  }
  entry->condition.notify_all();
}

void fill_bogus_cache_entry(std::shared_ptr<CacheRecord> entry) {
  addr n = entry->n;
  entry->postings = shared_array<addr>(n);
  entry->qostings = shared_array<addr>(n);
  entry->fostings = nullptr;
  for (addr i = 0; i < n; i++) {
    entry->postings.get()[i] = minfinity + 1 + i;
    entry->qostings.get()[i] = minfinity + 2 + i;
  }
  mark_cache_ready(entry);
}

void fill_hazel_cache_entry(std::shared_ptr<CacheRecord> entry,
                            std::shared_ptr<ReadGate> read_gate,
                            std::shared_ptr<SimplePostingFactory> factory,
                            addr offset, addr length) {
  std::string error;
  std::unique_ptr<char[]> bytes = read_gate->read(offset, length, &error);
  if (bytes != nullptr &&
      factory->cache_entry_from_compressed_blob(entry, bytes.get(), length,
                                                &error))
    return;
  assert(false);
  fill_bogus_cache_entry(entry);
}

template <typename Work> void async(Work work) {
  std::thread thread(work);
  thread.detach();
}

class HazelIdx final : public Idx {
public:
  static std::shared_ptr<Idx> make(const std::string &recipe,
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
                                "fvalue_compressor_recipe",
                                &fvalue_compressor, error))
      return nullptr;
    idx->posting_factory_ =
        SimplePostingFactory::make(posting_compressor, fvalue_compressor);
    if (!idx->load(error))
      return nullptr;
    return idx;
  }

  virtual ~HazelIdx(){};
  HazelIdx(const HazelIdx &) = delete;
  HazelIdx &operator=(const HazelIdx &) = delete;
  HazelIdx(HazelIdx &&) = delete;
  HazelIdx &operator=(HazelIdx &&) = delete;

  std::shared_ptr<CacheRecord> cache(addr feature) {
    size_t index;
    if (!locate_posting(directory_, feature, &index))
      return nullptr;
    addr start = posting_start(index);
    addr end = directory_[index].end;
    if (start == end)
      return singleton_cache(directory_[index].count_or_p);
    if (start > end) {
      assert(false);
      return nullptr;
    }
    bool created;
    std::shared_ptr<CacheRecord> entry = cache_record(index, &created);
    if (created)
      fill_hazel_cache_entry(entry, read_gate_, posting_factory_,
                             blob_offset_ + start, end - start);
    return entry;
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
      return std::make_unique<SingletonHopper>(directory_[index].count_or_p,
                                               directory_[index].count_or_p,
                                               0.0);
    if (start > end) {
      assert(false);
      return std::make_unique<EmptyHopper>();
    }
    bool created;
    std::shared_ptr<CacheRecord> entry = cache_record(index, &created);
    if (created) {
      std::shared_ptr<ReadGate> read_gate = read_gate_;
      std::shared_ptr<SimplePostingFactory> factory = posting_factory_;
      addr offset = blob_offset_ + start;
      addr length = end - start;
      async([entry, read_gate, factory, offset, length] {
        fill_hazel_cache_entry(entry, read_gate, factory, offset, length);
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

  addr posting_start(size_t index) {
    return index == 0 ? postings_start_ : directory_[index - 1].end;
  }

  std::shared_ptr<CacheRecord> singleton_cache(addr p) {
    std::shared_ptr<CacheRecord> entry =
        std::shared_ptr<CacheRecord>(new CacheRecord);
    entry->n = 1;
    entry->postings = shared_array<addr>(1);
    entry->qostings = entry->postings;
    entry->fostings = nullptr;
    *entry->postings = p;
    entry->ready = true;
    return entry;
  }

  std::shared_ptr<CacheRecord> cache_record(size_t index, bool *created) {
    addr feature = directory_[index].feature;
    std::lock_guard<std::mutex> lock(cache_lock_);
    auto cached = cache_.find(feature);
    if (cached != cache_.end()) {
      *created = false;
      return cached->second;
    }
    std::shared_ptr<CacheRecord> entry =
        std::shared_ptr<CacheRecord>(new CacheRecord);
    entry->n = directory_[index].count_or_p;
    entry->ready = false;
    cache_[feature] = entry;
    *created = true;
    return entry;
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
  std::map<addr, std::shared_ptr<CacheRecord>> cache_;
  std::mutex cache_lock_;
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

struct HazelMergeInput {
  std::string name;
  std::string filename;
  std::fstream in;
  std::string dna;
  std::map<std::string, std::string> parameters;
  std::map<std::string, HazelBlob> blobs;
  HazelBlob idx_blob;
  HazelBlob txt_blob;
  std::vector<HazelPostingEntry> idx_directory;
  std::vector<HazelTextEntry> txt_directory;
  addr idx_postings_start = 0;
  addr txt_chunk_space_start = 0;
  addr txt_directory_offset = 0;
  addr raw_text_length = 0;
  addr target_chunk_size = 0;
  addr sequence_start = 0;
  addr sequence_end = 0;
  size_t front = 0;

  bool open(std::shared_ptr<Working> working, const std::string &hazel,
            std::string *error) {
    name = hazel;
    filename = working->make_name(hazel);
    in.open(filename, std::ios::binary | std::ios::in);
    if (in.fail()) {
      safe_error(error) = "Hazel can't open: " + filename;
      return false;
    }
    return true;
  }

  bool read_at(addr where, char *data, addr length, std::string *error) {
    if (where < 0 || length < 0) {
      safe_error(error) = "Hazel got bad read range";
      return false;
    }
    if (length == 0)
      return true;
    in.clear();
    in.seekg(where, in.beg);
    if (in.fail()) {
      safe_error(error) = "Hazel can't seek in: " + filename;
      return false;
    }
    in.read(data, length);
    if (in.fail()) {
      safe_error(error) = "Hazel can't read from: " + filename;
      return false;
    }
    return true;
  }

  bool read_at(addr where, addr length, std::string *bytes,
               std::string *error) {
    bytes->assign(length, '\0');
    if (length == 0)
      return true;
    return read_at(where, &(*bytes)[0], length, error);
  }

  bool read_file_header(std::string *error) {
    const std::string magic = "#COTTONTAIL\n";
    std::string actual(magic.size(), '\0');
    in.read(&actual[0], actual.size());
    if (actual != magic) {
      safe_error(error) = "Hazel got bad single-file magic";
      return false;
    }
    std::ostringstream out;
    std::string line;
    while (std::getline(in, line)) {
      if (line == "") {
        dna = out.str();
        return true;
      }
      out << line << "\n";
    }
    safe_error(error) = "Hazel file has no DNA terminator";
    return false;
  }

  bool read_blob_dictionary(std::string *error) {
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
      std::string blob_name(name_length, '\0');
      in.read(&blob_name[0], name_length);
      HazelBlob blob;
      blob.name = blob_name;
      if (in.fail() || !read_pod(&in, &blob.offset) ||
          !read_pod(&in, &blob.length) || blob.offset < 0 ||
          blob.length < 0) {
        safe_error(error) = "Hazel got bad blob dictionary entry";
        return false;
      }
      blobs[blob_name] = blob;
    }
    auto idx = blobs.find("idx");
    auto txt = blobs.find("txt");
    if (idx == blobs.end() || txt == blobs.end()) {
      safe_error(error) = "Hazel missing idx or txt blob";
      return false;
    }
    idx_blob = idx->second;
    txt_blob = txt->second;
    return true;
  }

  bool read_idx_directory(std::string *error) {
    const std::string magic = hazel_idx_magic;
    addr header_length = magic.size() + 3 * sizeof(addr);
    idx_postings_start = header_length;
    if (idx_blob.length < header_length) {
      safe_error(error) = "Hazel idx blob is too short";
      return false;
    }
    std::string header;
    if (!read_at(idx_blob.offset, header_length, &header, error))
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
    if (directory_offset < header_length || directory_length < 0 ||
        directory_count < 0 ||
        directory_length != directory_count * (addr)(3 * sizeof(addr)) ||
        directory_offset > idx_blob.length ||
        directory_length > idx_blob.length - directory_offset) {
      safe_error(error) = "Hazel got bad idx directory";
      return false;
    }
    std::string directory;
    if (!read_at(idx_blob.offset + directory_offset, directory_length,
                 &directory, error))
      return false;
    idx_directory.reserve(directory_count);
    p = directory.data();
    for (addr i = 0; i < directory_count; i++) {
      HazelPostingEntry entry;
      entry.feature = read_pod<addr>(p);
      p += sizeof(addr);
      entry.end = read_pod<addr>(p);
      p += sizeof(addr);
      entry.count_or_p = read_pod<addr>(p);
      p += sizeof(addr);
      if (entry.end < header_length || entry.end > directory_offset ||
          (!idx_directory.empty() &&
           (entry.feature <= idx_directory.back().feature ||
            entry.end < idx_directory.back().end))) {
        safe_error(error) = "Hazel got bad idx posting boundary";
        return false;
      }
      idx_directory.push_back(entry);
    }
    return true;
  }

  bool read_txt_directory(std::string *error) {
    const std::string magic = hazel_txt_magic;
    addr header_length = magic.size() + 5 * sizeof(addr);
    if (txt_blob.length < header_length) {
      safe_error(error) = "Hazel txt blob is too short";
      return false;
    }
    std::string header;
    if (!read_at(txt_blob.offset, header_length, &header, error))
      return false;
    if (header.compare(0, magic.size(), magic) != 0) {
      safe_error(error) = "Hazel got bad txt blob magic";
      return false;
    }
    const char *p = header.data() + magic.size();
    addr directory_offset = read_pod<addr>(p);
    p += sizeof(addr);
    addr directory_length = read_pod<addr>(p);
    p += sizeof(addr);
    addr directory_count = read_pod<addr>(p);
    p += sizeof(addr);
    raw_text_length = read_pod<addr>(p);
    p += sizeof(addr);
    target_chunk_size = read_pod<addr>(p);
    txt_chunk_space_start = txt_blob.offset + header_length;
    txt_directory_offset = directory_offset;
    addr chunk_space_length = txt_blob.length - header_length;
    if (directory_offset < 0 || directory_length < 0 || directory_count < 0 ||
        raw_text_length < 0 || target_chunk_size <= 0 ||
        directory_length != directory_count * (addr)(2 * sizeof(addr)) ||
        directory_offset > chunk_space_length ||
        directory_length > chunk_space_length - directory_offset) {
      safe_error(error) = "Hazel got bad txt directory";
      return false;
    }
    if (directory_count == 0) {
      if (raw_text_length != 0 || directory_length != 0 ||
          directory_offset != 0) {
        safe_error(error) = "Hazel got bad empty txt directory";
        return false;
      }
      return true;
    }
    std::string directory;
    if (!read_at(txt_chunk_space_start + directory_offset, directory_length,
                 &directory, error))
      return false;
    txt_directory.reserve(directory_count);
    p = directory.data();
    for (addr i = 0; i < directory_count; i++) {
      HazelTextEntry entry;
      entry.raw_byte_end = read_pod<addr>(p);
      p += sizeof(addr);
      entry.compressed_byte_end = read_pod<addr>(p);
      p += sizeof(addr);
      if (entry.raw_byte_end < 0 || entry.compressed_byte_end < 0 ||
          entry.compressed_byte_end > directory_offset ||
          (!txt_directory.empty() &&
           (entry.raw_byte_end < txt_directory.back().raw_byte_end ||
            entry.compressed_byte_end <
                txt_directory.back().compressed_byte_end))) {
        safe_error(error) = "Hazel got bad txt chunk boundary";
        return false;
      }
      txt_directory.push_back(entry);
    }
    if (txt_directory.back().raw_byte_end != raw_text_length ||
        txt_directory.back().compressed_byte_end != directory_offset) {
      safe_error(error) = "Hazel got bad txt final boundary";
      return false;
    }
    return true;
  }

  bool read_sequence(std::string *error) {
    auto hazel = parameters.find("hazel");
    if (hazel == parameters.end()) {
      safe_error(error) = "Hazel DNA has no hazel metadata";
      return false;
    }
    std::map<std::string, std::string> metadata;
    if (!cook(hazel->second, &metadata, error))
      return false;
    auto start = metadata.find("sequence_start");
    auto end = metadata.find("sequence_end");
    if (start == metadata.end() || end == metadata.end()) {
      safe_error(error) = "Hazel DNA has no sequence range";
      return false;
    }
    try {
      sequence_start = std::stoll(start->second);
      sequence_end = std::stoll(end->second);
    } catch (...) {
      safe_error(error) = "Hazel DNA has bad sequence range";
      return false;
    }
    return true;
  }

  bool load(std::shared_ptr<Working> working, const std::string &hazel,
            std::string *error) {
    addr filename_start, filename_end;
    if (!parse_hazel_name(hazel, &filename_start, &filename_end)) {
      safe_error(error) = "Hazel merge got bad shard name: " + hazel;
      return false;
    }
    if (!open(working, hazel, error) || !read_file_header(error) ||
        !cook(dna, &parameters, error) || !read_blob_dictionary(error) ||
        !read_idx_directory(error) || !read_txt_directory(error) ||
        !read_sequence(error))
      return false;
    auto warren = parameters.find("warren");
    if (warren == parameters.end() || warren->second != "hazel") {
      safe_error(error) = "Hazel merge got non-Hazel DNA";
      return false;
    }
    if (filename_start != sequence_start || filename_end != sequence_end) {
      safe_error(error) = "Hazel filename and DNA sequence ranges differ";
      return false;
    }
    return true;
  }

  bool empty() const { return front >= idx_directory.size(); }

  addr front_feature() const {
    return empty() ? maxfinity : idx_directory[front].feature;
  }

  void skip_front() {
    if (!empty())
      front++;
  }

  addr posting_start(size_t index) const {
    return index == 0 ? idx_postings_start : idx_directory[index - 1].end;
  }

  std::shared_ptr<SimplePosting>
  posting_at(size_t index, std::shared_ptr<SimplePostingFactory> factory,
             std::string *error) {
    const HazelPostingEntry &entry = idx_directory[index];
    addr start = posting_start(index);
    if (start == entry.end) {
      std::shared_ptr<SimplePosting> posting =
          factory->posting_from_feature(entry.feature);
      posting->push(entry.count_or_p, entry.count_or_p, 0.0);
      return posting;
    }
    if (start > entry.end) {
      safe_error(error) = "Hazel got bad idx posting boundary";
      return nullptr;
    }
    std::string bytes;
    if (!read_at(idx_blob.offset + start, entry.end - start, &bytes, error))
      return nullptr;
    auto posting = factory->posting_from_compressed_blob(bytes.data(),
                                                         bytes.size(), error);
    if (posting == nullptr)
      return nullptr;
    if (posting->feature() != entry.feature) {
      safe_error(error) = "Hazel posting feature differs from directory";
      return nullptr;
    }
    return posting;
  }

  std::shared_ptr<SimplePosting>
  front_posting(std::shared_ptr<SimplePostingFactory> factory,
                std::string *error) {
    return posting_at(front, factory, error);
  }
};

std::string hazel_blob_dictionary(const std::vector<HazelBlob> &blobs) {
  std::ostringstream out(std::ios::out | std::ios::binary);
  const std::string magic = hazel_blob_dictionary_magic;
  out.write(magic.data(), magic.size());
  addr n = blobs.size();
  write_pod(&out, n);
  for (auto &blob : blobs) {
    addr name_length = blob.name.size();
    write_pod(&out, name_length);
    out.write(blob.name.data(), blob.name.size());
    write_pod(&out, blob.offset);
    write_pod(&out, blob.length);
  }
  return out.str();
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
    const std::string file_header = "#COTTONTAIL\n";
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

  bool write_posting(std::shared_ptr<SimplePosting> posting,
                     std::vector<HazelPostingEntry> *directory,
                     addr blob_start, std::string *error) {
    if (posting == nullptr || posting->size() == 0)
      return true;
    HazelPostingEntry entry;
    entry.feature = posting->feature();
    entry.end = (addr)out.tellp() - blob_start;
    entry.count_or_p = posting->size();
    addr p, q;
    fval v;
    if (entry.count_or_p == 1 && posting->get(0, &p, &q, &v) && p == q &&
        v == 0.0) {
      entry.count_or_p = p;
    } else {
      posting->write(&out);
      entry.end = (addr)out.tellp() - blob_start;
      if (out.fail()) {
        safe_error(error) = "Hazel merge failed to write idx postings";
        return false;
      }
    }
    directory->push_back(entry);
    return true;
  }

  bool copy_posting(std::shared_ptr<HazelMergeInput> input, size_t index,
                    std::vector<HazelPostingEntry> *directory, addr blob_start,
                    std::string *error) {
    const HazelPostingEntry &source = input->idx_directory[index];
    addr start = input->posting_start(index);
    if (start > source.end) {
      safe_error(error) = "Hazel got bad idx posting boundary";
      return false;
    }
    HazelPostingEntry entry;
    entry.feature = source.feature;
    entry.end = (addr)out.tellp() - blob_start;
    entry.count_or_p = source.count_or_p;
    if (start < source.end) {
      std::string bytes;
      if (!input->read_at(input->idx_blob.offset + start, source.end - start,
                          &bytes, error))
        return false;
      out.write(bytes.data(), bytes.size());
      entry.end = (addr)out.tellp() - blob_start;
      if (out.fail()) {
        safe_error(error) = "Hazel merge failed to copy idx postings";
        return false;
      }
    }
    directory->push_back(entry);
    return true;
  }

  bool write_txt(const std::vector<std::shared_ptr<HazelMergeInput>> &inputs,
                 addr target_chunk_size, std::string *error) {
    HazelBlob &blob = blobs[1];
    blob.offset = out.tellp();
    const std::string magic = hazel_txt_magic;
    out.write(magic.data(), magic.size());
    write_pod<addr>(&out, 0);
    write_pod<addr>(&out, 0);
    write_pod<addr>(&out, 0);
    write_pod<addr>(&out, 0);
    write_pod(&out, target_chunk_size);
    addr chunk_space_start = out.tellp();

    std::vector<HazelTextEntry> directory;
    addr raw_base = 0;
    for (auto &input : inputs) {
      addr previous_raw = 0;
      addr previous_compressed = 0;
      for (auto &entry : input->txt_directory) {
        addr raw_length = entry.raw_byte_end - previous_raw;
        addr compressed_length = entry.compressed_byte_end - previous_compressed;
        if (raw_length < 0 || compressed_length < 0) {
          safe_error(error) = "Hazel got bad txt chunk boundary";
          return false;
        }
        std::string compressed;
        if (!input->read_at(input->txt_chunk_space_start + previous_compressed,
                            compressed_length, &compressed, error))
          return false;
        out.write(compressed.data(), compressed.size());
        HazelTextEntry out_entry;
        out_entry.raw_byte_end = raw_base + entry.raw_byte_end;
        out_entry.compressed_byte_end = (addr)out.tellp() - chunk_space_start;
        directory.push_back(out_entry);
        previous_raw = entry.raw_byte_end;
        previous_compressed = entry.compressed_byte_end;
      }
      raw_base += input->raw_text_length;
    }

    addr directory_offset = (addr)out.tellp() - chunk_space_start;
    addr directory_count = directory.size();
    for (auto &entry : directory) {
      write_pod(&out, entry.raw_byte_end);
      write_pod(&out, entry.compressed_byte_end);
    }
    addr directory_length =
        (addr)out.tellp() - (chunk_space_start + directory_offset);
    blob.length = (addr)out.tellp() - blob.offset;
    out.seekp(blob.offset + magic.size());
    write_pod(&out, directory_offset);
    write_pod(&out, directory_length);
    write_pod(&out, directory_count);
    write_pod(&out, raw_base);
    write_pod(&out, target_chunk_size);
    out.seekp(blob.offset + blob.length);
    if (out.fail()) {
      safe_error(error) = "Hazel merge failed to write txt blob";
      return false;
    }
    return true;
  }

  bool write_idx(
      std::vector<std::shared_ptr<HazelMergeInput>> *inputs,
      addr text_chunk_feature, std::shared_ptr<SimplePosting> exclude,
      std::shared_ptr<SimplePosting> text_chunk,
      std::shared_ptr<SimplePostingFactory> factory, std::string *error) {
    HazelBlob &blob = blobs[0];
    blob.offset = out.tellp();
    const std::string magic = hazel_idx_magic;
    out.write(magic.data(), magic.size());
    write_pod<addr>(&out, 0);
    write_pod<addr>(&out, 0);
    write_pod<addr>(&out, 0);
    addr blob_start = blob.offset;

    std::vector<HazelPostingEntry> directory;
    bool wrote_null = false;
    bool wrote_text_chunk = false;
    while (true) {
      addr next = maxfinity;
      for (auto &input : *inputs)
        next = std::min(next, input->front_feature());
      if (!wrote_null && exclude != nullptr)
        next = std::min(next, null_feature);
      if (!wrote_text_chunk && text_chunk != nullptr)
        next = std::min(next, text_chunk_feature);
      if (next == maxfinity)
        break;

      std::vector<std::pair<std::shared_ptr<HazelMergeInput>, size_t>> sources;
      for (auto &input : *inputs) {
        if (input->front_feature() == next) {
          sources.emplace_back(input, input->front);
          input->skip_front();
        }
      }

      if (next == null_feature) {
        if (!write_posting(exclude, &directory, blob_start, error))
          return false;
        wrote_null = true;
      } else if (next == text_chunk_feature) {
        if (!write_posting(text_chunk, &directory, blob_start, error))
          return false;
        wrote_text_chunk = true;
      } else if (sources.size() == 1 && exclude == nullptr) {
        if (!copy_posting(sources[0].first, sources[0].second, &directory,
                          blob_start, error))
          return false;
      } else if (!sources.empty()) {
        std::vector<std::shared_ptr<SimplePosting>> postings;
        for (auto &source : sources) {
          auto posting = source.first->posting_at(source.second, factory, error);
          if (posting == nullptr)
            return false;
          postings.push_back(posting);
        }
        auto posting = factory->posting_from_merge(postings, exclude);
        if (!write_posting(posting, &directory, blob_start, error))
          return false;
      }
    }

    addr directory_offset = (addr)out.tellp() - blob_start;
    addr directory_count = directory.size();
    for (auto &entry : directory) {
      write_pod(&out, entry.feature);
      write_pod(&out, entry.end);
      write_pod(&out, entry.count_or_p);
    }
    addr directory_length = (addr)out.tellp() - blob_start - directory_offset;
    blob.length = (addr)out.tellp() - blob_start;
    out.seekp(blob.offset + magic.size());
    write_pod(&out, directory_offset);
    write_pod(&out, directory_length);
    write_pod(&out, directory_count);
    out.seekp(blob.offset + blob.length);
    if (out.fail()) {
      safe_error(error) = "Hazel merge failed to write idx blob";
      return false;
    }
    return true;
  }
};

bool normalize_dna_for_merge(const std::map<std::string, std::string> &input,
                             std::string *normalized, std::string *error) {
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
  auto txt = parameters.find("txt");
  if (txt != parameters.end()) {
    std::map<std::string, std::string> txt_package;
    if (!cook(txt->second, &txt_package, error))
      return false;
    auto recipe = txt_package.find("recipe");
    if (recipe != txt_package.end()) {
      std::map<std::string, std::string> txt_recipe;
      if (!cook(recipe->second, &txt_recipe, error))
        return false;
      txt_recipe.erase("chunk_size");
      recipe->second = freeze(txt_recipe);
      txt->second = freeze(txt_package);
    }
  }
  *normalized = freeze(parameters);
  return true;
}

bool merged_hazel_dna(const HazelMergeInput &first, addr sequence_start,
                      addr sequence_end, addr target_chunk_size,
                      const std::string &parameters_recipe, std::string *dna,
                      std::string *error) {
  std::map<std::string, std::string> parameters = first.parameters;
  std::map<std::string, std::string> metadata;
  auto hazel = parameters.find("hazel");
  if (hazel != parameters.end() && !cook(hazel->second, &metadata, error))
    return false;
  metadata["sequence_start"] = std::to_string(sequence_start);
  metadata["sequence_end"] = std::to_string(sequence_end);
  parameters["hazel"] = freeze(metadata);
  if (parameters_recipe == "")
    parameters.erase("parameters");
  else
    parameters["parameters"] = parameters_recipe;

  auto txt = parameters.find("txt");
  if (txt == parameters.end()) {
    safe_error(error) = "Hazel DNA has no txt recipe";
    return false;
  }
  std::map<std::string, std::string> txt_package;
  if (!cook(txt->second, &txt_package, error))
    return false;
  auto recipe = txt_package.find("recipe");
  if (recipe == txt_package.end()) {
    safe_error(error) = "Hazel DNA has no txt subrecipe";
    return false;
  }
  std::map<std::string, std::string> txt_recipe;
  if (!cook(recipe->second, &txt_recipe, error))
    return false;
  txt_recipe["chunk_size"] = std::to_string(target_chunk_size);
  recipe->second = freeze(txt_recipe);
  txt->second = freeze(txt_package);
  *dna = freeze(parameters);
  return true;
}

bool posting_for_feature(
    const std::vector<std::shared_ptr<HazelMergeInput>> &inputs, addr feature,
    std::shared_ptr<SimplePostingFactory> factory,
    std::shared_ptr<SimplePosting> *posting, std::string *error) {
  std::vector<std::shared_ptr<SimplePosting>> postings;
  for (auto &input : inputs) {
    size_t index;
    if (locate_posting(input->idx_directory, feature, &index)) {
      auto p = input->posting_at(index, factory, error);
      if (p == nullptr)
        return false;
      postings.push_back(p);
    }
  }
  if (postings.empty()) {
    *posting = nullptr;
  } else if (postings.size() == 1) {
    *posting = postings[0];
  } else {
    *posting = factory->posting_from_merge(postings);
  }
  return true;
}

bool text_chunk_posting(
    const std::vector<std::shared_ptr<HazelMergeInput>> &inputs, addr feature,
    std::shared_ptr<SimplePostingFactory> factory,
    std::shared_ptr<SimplePosting> *posting, std::string *error) {
  *posting = factory->posting_from_feature(feature);
  addr raw_base = 0;
  for (auto &input : inputs) {
    size_t index;
    if (locate_posting(input->idx_directory, feature, &index)) {
      auto source = input->posting_at(index, factory, error);
      if (source == nullptr)
        return false;
      addr p, q;
      fval v;
      for (size_t i = 0; i < source->size(); i++) {
        source->get(i, &p, &q, &v);
        (*posting)->push(p, q, addr2fval(fval2addr(v) + raw_base));
      }
    }
    raw_base += input->raw_text_length;
  }
  if ((*posting)->size() == 0)
    *posting = nullptr;
  return true;
}

bool posting_factory_from_idx_recipe(
    const std::string &idx_recipe,
    std::shared_ptr<SimplePostingFactory> *factory, std::string *error) {
  std::shared_ptr<Compressor> posting_compressor;
  std::shared_ptr<Compressor> fvalue_compressor;
  if (!compressor_from_recipe(idx_recipe, "posting_compressor",
                              "posting_compressor_recipe",
                              &posting_compressor, error) ||
      !compressor_from_recipe(idx_recipe, "fvalue_compressor",
                              "fvalue_compressor_recipe", &fvalue_compressor,
                              error))
    return false;
  *factory = SimplePostingFactory::make(posting_compressor, fvalue_compressor);
  return *factory != nullptr;
}

} // namespace

bool Hazel::merge(std::shared_ptr<Working> working,
                  const std::vector<std::string> &hazels,
                  const std::string &parameters, std::string *error) {
  if (working == nullptr) {
    safe_error(error) = "Hazel merge needs a working directory";
    return false;
  }
  if (hazels.empty()) {
    safe_error(error) = "Hazel merge got no shards";
    return false;
  }

  std::vector<std::shared_ptr<HazelMergeInput>> inputs;
  std::string normalized;
  for (size_t i = 0; i < hazels.size(); i++) {
    auto input = std::make_shared<HazelMergeInput>();
    if (!input->load(working, hazels[i], error))
      return false;
    if (i > 0 && inputs.back()->sequence_end + 1 != input->sequence_start) {
      safe_error(error) = "Hazel merge got non-contiguous shards";
      return false;
    }
    std::string current;
    if (!normalize_dna_for_merge(input->parameters, &current, error))
      return false;
    if (i == 0)
      normalized = current;
    else if (current != normalized) {
      safe_error(error) = "Hazel merge got incompatible DNA";
      return false;
    }
    inputs.push_back(input);
  }

  std::string featurizer_name, featurizer_recipe;
  std::string idx_name, idx_recipe;
  if (!name_and_recipe(inputs[0]->parameters, "featurizer", &featurizer_name,
                       &featurizer_recipe, error) ||
      !name_and_recipe(inputs[0]->parameters, "idx", &idx_name, &idx_recipe,
                       error))
    return false;
  if (idx_name != "hazel") {
    safe_error(error) = "Hazel merge got non-Hazel idx";
    return false;
  }

  std::shared_ptr<Featurizer> featurizer =
      Featurizer::make(featurizer_name, featurizer_recipe, error);
  if (featurizer == nullptr)
    return false;
  addr text_chunk_feature = featurizer->featurize(text_chunk_tag);

  std::shared_ptr<SimplePostingFactory> posting_factory;
  if (!posting_factory_from_idx_recipe(idx_recipe, &posting_factory, error))
    return false;

  addr target_chunk_size = inputs[0]->target_chunk_size;
  for (auto &input : inputs)
    target_chunk_size = std::min(target_chunk_size, input->target_chunk_size);

  std::shared_ptr<SimplePosting> exclude;
  std::shared_ptr<SimplePosting> text_chunk;
  if (!posting_for_feature(inputs, null_feature, posting_factory, &exclude,
                           error) ||
      !text_chunk_posting(inputs, text_chunk_feature, posting_factory,
                          &text_chunk, error))
    return false;

  std::string dna;
  if (!merged_hazel_dna(*inputs[0], inputs.front()->sequence_start,
                        inputs.back()->sequence_end, target_chunk_size,
                        parameters, &dna, error))
    return false;

  std::string tempname = working->make_temp("hazel");
  HazelMergeOutput output;
  if (!output.open(tempname, dna, error) ||
      !output.write_txt(inputs, target_chunk_size, error) ||
      !output.write_idx(&inputs, text_chunk_feature, exclude, text_chunk,
                        posting_factory, error) ||
      !output.close(error)) {
    std::remove(tempname.c_str());
    return false;
  }

  std::string final_name = hazel_default_name(inputs.front()->sequence_start,
                                              inputs.back()->sequence_end);
  std::string final_filename = working->make_name(final_name);
  if (link(tempname.c_str(), final_filename.c_str()) != 0) {
    safe_error(error) = "Hazel merge can't link shard: " + final_filename;
    std::remove(tempname.c_str());
    return false;
  }
  std::remove(tempname.c_str());
  return true;
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
      HazelIdx::make(idx_recipe, filename, file, idx_blob->second, error);
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
