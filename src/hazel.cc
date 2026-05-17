#include "src/hazel.h"

#include <algorithm>
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
  addr raw_end;
  addr compressed_end;
};

class HazelTxt final : public Txt {
public:
  static std::shared_ptr<Txt> make(const std::string &recipe,
                                   std::shared_ptr<HazelFile> file,
                                   const HazelBlob &blob,
                                   std::shared_ptr<Idx> idx,
                                   std::shared_ptr<Featurizer> featurizer,
                                   std::shared_ptr<Tokenizer> tokenizer,
                                   std::string *error = nullptr) {
    std::shared_ptr<HazelTxt> txt = std::shared_ptr<HazelTxt>(new HazelTxt());
    txt->therecipe_ = recipe;
    txt->file_ = file;
    txt->blob_offset_ = blob.offset;
    txt->blob_length_ = blob.length;
    txt->idx_ = idx;
    txt->featurizer_ = featurizer;
    txt->tokenizer_ = tokenizer;
    txt->count_ = -1;
    txt->range_valid_ = false;
    if (!compressor_from_recipe(recipe, "compressor", "compressor_recipe",
                                &txt->compressor_, error))
      return nullptr;
    if (!txt->load(error))
      return nullptr;
    txt->hopper_ = idx->hopper(featurizer->featurize(text_chunk_tag));
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
    return HazelTxt::make(therecipe_, file_,
                          HazelBlob{"txt", blob_offset_, blob_length_}, idx_,
                          featurizer_, tokenizer_, error);
  }
  std::string translate_(addr p, addr q) final {
    if (p < 0)
      p = 0;
    if (p == maxfinity || q < p)
      return "";
    addr p0, q0, i;
    addr p1, q1, j;
    std::vector<std::shared_ptr<const std::string>> chunks;
    std::vector<addr> raw_starts;
    std::vector<addr> raw_ends;
    addr raw_start, raw_end;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      hopper_->rho(p, &p0, &q0, &i);
      if (p0 == maxfinity || p0 > q)
        return "";
      hopper_->ohr(q, &p1, &q1, &j);
      raw_start = i;
      raw_end = q > q1 ? raw_text_length_ : j;
      if (p0 < p)
        raw_start = skip_in_cache(raw_start, p - p0);
      else
        p = p0;
      if (q <= q1) {
        if (q0 == q1)
          raw_end = skip_in_cache(raw_start, q - p + 1);
        else
          raw_end = skip_in_cache(raw_end, q - p1 + 1);
      }
      if (raw_start < 0)
        raw_start = 0;
      if (raw_end > raw_text_length_)
        raw_end = raw_text_length_;
      if (raw_end < raw_start)
        return "";
      if (!cache_range(raw_start, raw_end, &chunks, &raw_starts, &raw_ends))
        return "";
    }
    std::string result;
    result.reserve(raw_end - raw_start);
    for (size_t k = 0; k < chunks.size(); k++) {
      addr start = std::max(raw_start, raw_starts[k]);
      addr end = std::min(raw_end, raw_ends[k]);
      if (start < end)
        result.append(chunks[k]->data() + (start - raw_starts[k]),
                      end - start);
    }
    return result;
  };
  std::string raw_(addr p, addr q) final { return translate(p, q); };
  addr tokens_() final {
    std::lock_guard<std::mutex> lock(mutex_);
    if (count_ >= 0)
      return count_;
    count_ = 0;
    addr p, q;
    for (hopper_->tau(0, &p, &q); p < maxfinity; hopper_->tau(p + 1, &p, &q))
      count_ += q - p + 1;
    return count_;
  };
  bool range_(addr *p, addr *q) final {
    if (!range_valid_) {
      std::lock_guard<std::mutex> lock(mutex_);
      if (!range_valid_) {
        range_valid_ = true;
        hopper_->tau(minfinity + 1, &range_p_, &range_q_);
        if (range_p_ < maxfinity) {
          addr ignore;
          hopper_->uat(maxfinity - 1, &ignore, &range_q_);
        }
      }
    }
    *p = range_p_;
    *q = range_q_;
    return range_p_ < maxfinity;
  }

  bool load(std::string *error) {
    const std::string magic = "COTTONTAIL_HAZEL_TXT_V1\n";
    chunks_start_ = magic.size() + 5 * sizeof(addr);
    if (blob_length_ < chunks_start_) {
      safe_error(error) = "Hazel txt blob is too short";
      return false;
    }
    std::string header(chunks_start_, '\0');
    if (!file_->read(blob_offset_, &header[0], header.size(), error))
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
    raw_text_length_ = read_pod<addr>(p);
    p += sizeof(addr);
    target_chunk_size_ = read_pod<addr>(p);
    if (directory_offset < chunks_start_ || directory_length < 0 ||
        directory_count < 0 || raw_text_length_ < 0 ||
        target_chunk_size_ <= 0 ||
        directory_offset + directory_length > blob_length_ ||
        directory_length != directory_count * (addr)(2 * sizeof(addr))) {
      safe_error(error) = "Hazel got bad txt directory";
      return false;
    }
    std::string bytes(directory_length, '\0');
    if (!file_->read(blob_offset_ + directory_offset, &bytes[0], bytes.size(),
                     error))
      return false;
    directory_.reserve(directory_count);
    p = bytes.data();
    for (addr i = 0; i < directory_count; i++) {
      HazelTextEntry entry;
      entry.raw_end = read_pod<addr>(p);
      p += sizeof(addr);
      entry.compressed_end = read_pod<addr>(p);
      p += sizeof(addr);
      if (entry.raw_end < 0 || entry.raw_end > raw_text_length_ ||
          entry.compressed_end < chunks_start_ ||
          entry.compressed_end > directory_offset ||
          (!directory_.empty() &&
           (entry.raw_end < directory_.back().raw_end ||
            entry.compressed_end < directory_.back().compressed_end))) {
        safe_error(error) = "Hazel got bad txt chunk boundary";
        return false;
      }
      directory_.push_back(entry);
    }
    return true;
  }

  bool chunk_bounds(size_t chunk, addr *raw_start, addr *raw_end,
                    addr *compressed_start, addr *compressed_end) {
    if (chunk >= directory_.size())
      return false;
    *raw_start = chunk == 0 ? 0 : directory_[chunk - 1].raw_end;
    *raw_end = directory_[chunk].raw_end;
    *compressed_start =
        chunk == 0 ? chunks_start_ : directory_[chunk - 1].compressed_end;
    *compressed_end = directory_[chunk].compressed_end;
    return true;
  }

  size_t chunk_for_raw(addr raw) {
    auto it = std::upper_bound(
        directory_.begin(), directory_.end(), raw,
        [](addr raw, const HazelTextEntry &entry) {
          return raw < entry.raw_end;
        });
    if (it == directory_.end())
      return directory_.size();
    return it - directory_.begin();
  }

  std::shared_ptr<const std::string> chunk(size_t k) {
    auto cached = cache_.find(k);
    if (cached != cache_.end())
      return cached->second;
    addr raw_start, raw_end, compressed_start, compressed_end;
    if (!chunk_bounds(k, &raw_start, &raw_end, &compressed_start,
                      &compressed_end))
      return nullptr;
    std::string compressed(compressed_end - compressed_start, '\0');
    std::string error;
    if (!file_->read(blob_offset_ + compressed_start, &compressed[0],
                     compressed.size(), &error)) {
      assert(false);
      return nullptr;
    }
    std::shared_ptr<std::string> raw = std::make_shared<std::string>();
    raw->resize(raw_end - raw_start);
    addr n = compressor_->tang(&compressed[0], compressed.size(), &(*raw)[0],
                               raw->size());
    if (n != (addr)raw->size()) {
      assert(false);
      return nullptr;
    }
    cache_[k] = raw;
    return raw;
  }

  bool cache_range(addr raw_start, addr raw_end,
                   std::vector<std::shared_ptr<const std::string>> *chunks,
                   std::vector<addr> *raw_starts,
                   std::vector<addr> *raw_ends) {
    if (raw_start == raw_end)
      return true;
    size_t first = chunk_for_raw(raw_start);
    size_t last = chunk_for_raw(raw_end - 1);
    if (first >= directory_.size() || last >= directory_.size())
      return false;
    for (size_t k = first; k <= last; k++) {
      addr rs, re, cs, ce;
      if (!chunk_bounds(k, &rs, &re, &cs, &ce))
        return false;
      std::shared_ptr<const std::string> c = chunk(k);
      if (c == nullptr)
        return false;
      chunks->push_back(c);
      raw_starts->push_back(rs);
      raw_ends->push_back(re);
    }
    return true;
  }

  addr skip_in_cache(addr raw, addr tokens) {
    if (tokens <= 0)
      return raw;
    size_t k = chunk_for_raw(raw);
    addr remaining = tokens;
    addr current = raw;
    while (k < directory_.size()) {
      addr raw_start, raw_end, compressed_start, compressed_end;
      if (!chunk_bounds(k, &raw_start, &raw_end, &compressed_start,
                        &compressed_end))
        return current;
      std::shared_ptr<const std::string> c = chunk(k);
      if (c == nullptr)
        return current;
      addr offset = std::max<addr>(0, current - raw_start);
      const char *start = c->data() + offset;
      const char *end =
          tokenizer_->skip(start, c->size() - offset, remaining);
      current = raw_start + (end - c->data());
      if (current < raw_end)
        return current;
      addr consumed = tokens_in_chunk(start, c->data() + c->size() - start);
      if (consumed >= remaining)
        return current;
      remaining -= consumed;
      if (remaining <= 0)
        return current;
      k++;
    }
    return raw_text_length_;
  }

  addr tokens_in_chunk(const char *text, addr length) {
    if (length <= 0)
      return 0;
    return tokenizer_->split(std::string(text, length)).size();
  }

  std::string therecipe_;
  std::shared_ptr<HazelFile> file_;
  addr blob_offset_;
  addr blob_length_;
  addr chunks_start_;
  addr raw_text_length_;
  addr target_chunk_size_;
  std::vector<HazelTextEntry> directory_;
  std::shared_ptr<Compressor> compressor_;
  std::shared_ptr<Idx> idx_;
  std::shared_ptr<Featurizer> featurizer_;
  std::shared_ptr<Tokenizer> tokenizer_;
  std::mutex mutex_;
  std::unique_ptr<Hopper> hopper_;
  std::map<size_t, std::shared_ptr<const std::string>> cache_;
  addr count_;
  bool range_valid_;
  addr range_p_, range_q_;
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
  std::shared_ptr<Txt> txt =
      Txt::wrap(txt_recipe,
                HazelTxt::make(txt_recipe, file, txt_blob->second, idx,
                               featurizer, tokenizer, error),
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
