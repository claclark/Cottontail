#include "src/hazel.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <system_error>
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

  static bool merge(const std::vector<std::shared_ptr<HazelIdx>> &idxs,
                    const std::vector<addr> &text_lengths,
                    addr text_chunk_feature, const std::string &pst_name,
                    const std::string &dct_name,
                    std::ostream *out, std::string *error = nullptr);

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
      posting->push(directory_[index].count_or_p,
                    directory_[index].count_or_p, 0.0);
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
};

constexpr addr hazel_posting_entry_size = 3 * sizeof(addr);

addr hazel_idx_header_length() {
  return hazel_idx_magic.size() + 3 * sizeof(addr);
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
    safe_error(error) = "Hazel merge can't create checkpoint: " + filename;
    return false;
  }
  out.close();
  return true;
}

bool hazel_truncate_file(const std::string &filename, addr size,
                         std::string *error) {
  if (size < 0) {
    safe_error(error) = "Hazel merge got negative checkpoint size";
    return false;
  }
  if (truncate(filename.c_str(), size) != 0) {
    safe_error(error) = "Hazel merge can't truncate checkpoint: " + filename;
    return false;
  }
  return true;
}

bool hazel_reset_checkpoints(const std::string &pst_name,
                             const std::string &dct_name,
                             std::string *error) {
  return hazel_reset_file(pst_name, error) &&
         hazel_reset_file(dct_name, error);
}

bool hazel_read_checkpoint_directory(const std::string &dct_name,
                                     std::vector<HazelPostingEntry> *directory,
                                     std::string *error) {
  addr size;
  if (!hazel_file_size(dct_name, &size))
    return false;
  addr usable = size - (size % hazel_posting_entry_size);
  if (usable != size && !hazel_truncate_file(dct_name, usable, error))
    return false;
  directory->clear();
  if (usable == 0)
    return true;
  std::string bytes(usable, '\0');
  std::fstream in(dct_name, std::ios::binary | std::ios::in);
  if (in.fail()) {
    safe_error(error) = "Hazel merge can't read checkpoint: " + dct_name;
    return false;
  }
  in.read(&bytes[0], bytes.size());
  if (in.fail()) {
    safe_error(error) = "Hazel merge got bad checkpoint: " + dct_name;
    return false;
  }
  const char *p = bytes.data();
  addr count = usable / hazel_posting_entry_size;
  directory->reserve(count);
  for (addr i = 0; i < count; i++) {
    HazelPostingEntry entry;
    entry.feature = read_pod<addr>(p);
    p += sizeof(addr);
    entry.end = read_pod<addr>(p);
    p += sizeof(addr);
    entry.count_or_p = read_pod<addr>(p);
    p += sizeof(addr);
    directory->push_back(entry);
  }
  return true;
}

bool hazel_repair_checkpoint(const std::string &pst_name,
                             const std::string &dct_name,
                             std::vector<HazelPostingEntry> *directory,
                             std::string *error) {
  addr pst_size;
  if (!hazel_file_size(pst_name, &pst_size))
    return false;
  if (!hazel_read_checkpoint_directory(dct_name, directory, error))
    return false;
  addr covered_end = hazel_idx_header_length() + pst_size;
  addr previous_end = hazel_idx_header_length();
  size_t keep = 0;
  for (size_t i = 0; i < directory->size(); i++) {
    const HazelPostingEntry &entry = (*directory)[i];
    if (entry.end < previous_end ||
        (i > 0 && entry.feature <= (*directory)[i - 1].feature)) {
      safe_error(error) = "Hazel merge got inconsistent idx checkpoint";
      break;
    }
    if (entry.end > covered_end)
      break;
    previous_end = entry.end;
    keep = i + 1;
  }
  if (keep < directory->size()) {
    directory->resize(keep);
    if (!hazel_truncate_file(dct_name, keep * hazel_posting_entry_size, error))
      return false;
  }
  addr wanted_pst_size =
      directory->empty() ? 0 : directory->back().end - hazel_idx_header_length();
  return hazel_truncate_file(pst_name, wanted_pst_size, error);
}

bool hazel_write_checkpoint_entry(std::fstream *dct,
                                  const HazelPostingEntry &entry,
                                  std::string *error) {
  write_pod(dct, entry.feature);
  write_pod(dct, entry.end);
  write_pod(dct, entry.count_or_p);
  dct->flush();
  if (dct->fail()) {
    safe_error(error) = "Hazel merge failed to write idx checkpoint";
    return false;
  }
  return true;
}

bool hazel_append_checkpoint_posting(
    std::fstream *pst, std::fstream *dct,
    std::shared_ptr<SimplePosting> posting, HazelPostingEntry *entry,
    bool *wrote, std::string *error) {
  *wrote = false;
  if (posting == nullptr || posting->size() == 0)
    return true;
  pst->seekp(0, pst->end);
  if (pst->fail()) {
    safe_error(error) = "Hazel merge can't seek idx checkpoint";
    return false;
  }
  addr start = (addr)pst->tellp() + hazel_idx_header_length();
  entry->feature = posting->feature();
  entry->end = start;
  entry->count_or_p = posting->size();
  addr p, q;
  fval v;
  if (entry->count_or_p == 1 && posting->get(0, &p, &q, &v) && p == q &&
      v == 0.0) {
    entry->count_or_p = p;
  } else {
    posting->write(pst);
    pst->flush();
    entry->end = (addr)pst->tellp() + hazel_idx_header_length();
    if (pst->fail()) {
      safe_error(error) = "Hazel merge failed to write idx postings";
      return false;
    }
  }
  dct->seekp(0, dct->end);
  if (dct->fail()) {
    safe_error(error) = "Hazel merge can't seek idx checkpoint";
    return false;
  }
  if (!hazel_write_checkpoint_entry(dct, *entry, error))
    return false;
  *wrote = true;
  return true;
}

std::shared_ptr<SimplePosting> hazel_checkpoint_posting(
    const std::string &pst_name, const HazelPostingEntry &entry,
    addr previous_end, std::shared_ptr<SimplePostingFactory> factory,
    std::string *error) {
  if (entry.end == previous_end) {
    std::shared_ptr<SimplePosting> posting =
        factory->posting_from_feature(entry.feature);
    posting->push(entry.count_or_p, entry.count_or_p, 0.0);
    return posting;
  }
  if (entry.end < previous_end) {
    safe_error(error) = "Hazel merge got bad checkpoint posting boundary";
    return nullptr;
  }
  addr offset = previous_end - hazel_idx_header_length();
  addr length = entry.end - previous_end;
  std::string bytes(length, '\0');
  std::fstream in(pst_name, std::ios::binary | std::ios::in);
  if (in.fail()) {
    safe_error(error) = "Hazel merge can't read checkpoint: " + pst_name;
    return nullptr;
  }
  in.seekg(offset, in.beg);
  in.read(&bytes[0], bytes.size());
  if (in.fail()) {
    safe_error(error) = "Hazel merge got bad checkpoint: " + pst_name;
    return nullptr;
  }
  auto posting =
      factory->posting_from_compressed_blob(bytes.data(), bytes.size(), error);
  if (posting == nullptr)
    return nullptr;
  if (posting->feature() != entry.feature) {
    safe_error(error) = "Hazel checkpoint posting feature differs from "
                        "directory";
    return nullptr;
  }
  return posting;
}

bool hazel_copy_checkpoint_bytes(const std::string &filename, addr length,
                                 std::ostream *out, std::string *error) {
  std::fstream in(filename, std::ios::binary | std::ios::in);
  if (in.fail()) {
    safe_error(error) = "Hazel merge can't read checkpoint: " + filename;
    return false;
  }
  std::vector<char> buffer(1 << 20);
  addr remaining = length;
  while (remaining > 0) {
    addr n = std::min<addr>(remaining, buffer.size());
    in.read(buffer.data(), n);
    if (in.fail()) {
      safe_error(error) = "Hazel merge got bad checkpoint: " + filename;
      return false;
    }
    out->write(buffer.data(), n);
    if (out->fail()) {
      safe_error(error) = "Hazel merge failed to write idx blob";
      return false;
    }
    remaining -= n;
  }
  return true;
}

bool HazelIdx::merge(const std::vector<std::shared_ptr<HazelIdx>> &idxs,
                     const std::vector<addr> &text_lengths,
                     addr text_chunk_feature, const std::string &pst_name,
                     const std::string &dct_name,
                     std::ostream *out, std::string *error) {
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
  if (pst_name == "" || dct_name == "") {
    safe_error(error) = "HazelIdx merge got empty checkpoint name";
    return false;
  }
  if (out == nullptr) {
    safe_error(error) = "HazelIdx merge got no output stream";
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
  const addr header_length = hazel_idx_header_length();

  std::vector<addr> text_bases;
  text_bases.reserve(text_lengths.size());
  addr text_base = 0;
  for (addr length : text_lengths) {
    text_bases.push_back(text_base);
    text_base += length;
  }

  auto has_source_feature = [&](addr feature) {
    for (auto &idx : idxs) {
      size_t index;
      if (locate_posting(idx->directory_, feature, &index))
        return true;
    }
    return false;
  };

  auto postings_for_feature = [&](addr feature,
                                  std::vector<std::shared_ptr<SimplePosting>>
                                      *postings,
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

  auto open_checkpoint_streams = [&](std::fstream *pst, std::fstream *dct,
                                     std::string *error) {
    pst->open(pst_name, std::ios::binary | std::ios::in | std::ios::out);
    if (pst->fail()) {
      safe_error(error) = "Hazel merge can't open checkpoint: " + pst_name;
      return false;
    }
    dct->open(dct_name, std::ios::binary | std::ios::in | std::ios::out);
    if (dct->fail()) {
      safe_error(error) = "Hazel merge can't open checkpoint: " + dct_name;
      return false;
    }
    pst->seekp(0, pst->end);
    dct->seekp(0, dct->end);
    if (pst->fail() || dct->fail()) {
      safe_error(error) = "Hazel merge can't seek idx checkpoint";
      return false;
    }
    return true;
  };

  // 1. Establish processing invariant.
  //
  // The checkpoint files exist, the dictionary is a whole number of
  // HazelPostingEntry records, the posting bytes end where the last dictionary
  // entry says they end, and null_feature has already been handled if any
  // input contains erasures.
  std::vector<HazelPostingEntry> checkpoint;
  std::string checkpoint_error;
  bool reset =
      !hazel_repair_checkpoint(pst_name, dct_name, &checkpoint,
                               &checkpoint_error);
  bool source_has_null = has_source_feature(null_feature);
  if (!reset && source_has_null &&
      (checkpoint.empty() || checkpoint.front().feature != null_feature))
    reset = true;
  if (!reset && !source_has_null && !checkpoint.empty() &&
      checkpoint.front().feature == null_feature)
    reset = true;
  if (reset) {
    if (!hazel_reset_checkpoints(pst_name, dct_name, error))
      return false;
    checkpoint.clear();
  } else if (!checkpoint.empty()) {
    addr wanted_pst_size = checkpoint.back().end - header_length;
    if (!hazel_truncate_file(pst_name, wanted_pst_size, error))
      return false;
  } else if (!hazel_truncate_file(pst_name, 0, error)) {
    return false;
  }

  std::shared_ptr<SimplePosting> exclude;
  if (!checkpoint.empty() && checkpoint.front().feature == null_feature) {
    exclude = hazel_checkpoint_posting(pst_name, checkpoint.front(),
                                       header_length, factory,
                                       &checkpoint_error);
    if (exclude == nullptr) {
      if (!hazel_reset_checkpoints(pst_name, dct_name, error))
        return false;
      checkpoint.clear();
    }
  }

  std::fstream pst;
  std::fstream dct;
  if (!open_checkpoint_streams(&pst, &dct, error))
    return false;

  if (checkpoint.empty() && source_has_null) {
    std::vector<std::shared_ptr<SimplePosting>> postings;
    if (!postings_for_feature(null_feature, &postings, error))
      return false;
    exclude = factory->posting_from_merge(postings);
    HazelPostingEntry entry;
    bool wrote;
    if (!hazel_append_checkpoint_posting(&pst, &dct, exclude, &entry, &wrote,
                                         error))
      return false;
    if (wrote)
      checkpoint.push_back(entry);
  }

  // 2. Process remaining features.
  //
  // Resume after the last completed dictionary feature. Posting semantics are
  // delegated to SimplePostingFactory; this loop only supplies the Hazel input
  // postings and appends completed checkpoint records.
  addr last_feature =
      checkpoint.empty() ? null_feature : checkpoint.back().feature;
  std::vector<size_t> positions(idxs.size(), 0);
  for (size_t i = 0; i < idxs.size(); i++)
    while (positions[i] < idxs[i]->directory_.size() &&
           idxs[i]->directory_[positions[i]].feature <= last_feature)
      positions[i]++;

  for (;;) {
    addr next = maxfinity;
    for (size_t i = 0; i < idxs.size(); i++)
      if (positions[i] < idxs[i]->directory_.size())
        next = std::min(next, idxs[i]->directory_[positions[i]].feature);
    if (next == maxfinity)
      break;

    for (size_t i = 0; i < idxs.size(); i++)
      if (positions[i] < idxs[i]->directory_.size() &&
          idxs[i]->directory_[positions[i]].feature == next)
        positions[i]++;

    std::shared_ptr<SimplePosting> posting;
    if (next == text_chunk_feature) {
      posting = text_chunk_posting();
      if (has_source_feature(text_chunk_feature) && posting == nullptr)
        return false;
    } else {
      std::vector<std::shared_ptr<SimplePosting>> postings;
      if (!postings_for_feature(next, &postings, error))
        return false;
      posting = factory->posting_from_merge(postings, exclude);
    }

    HazelPostingEntry entry;
    bool wrote;
    if (!hazel_append_checkpoint_posting(&pst, &dct, posting, &entry, &wrote,
                                         error))
      return false;
    if (wrote)
      checkpoint.push_back(entry);
    last_feature = next;
  }
  pst.close();
  dct.close();
  if (pst.fail() || dct.fail()) {
    safe_error(error) = "Hazel merge failed to close idx checkpoint";
    return false;
  }

  // 3. Finalize the blob.
  //
  // The checkpoint postings and dictionary already use blob-relative `end`
  // offsets, so finalization is just the Hazel idx header followed by those two
  // checkpoint files.
  addr final_pst_size;
  addr final_dct_size;
  if (!hazel_file_size(pst_name, &final_pst_size) ||
      !hazel_file_size(dct_name, &final_dct_size)) {
    safe_error(error) = "Hazel merge missing idx checkpoint";
    return false;
  }
  if (final_dct_size % hazel_posting_entry_size != 0) {
    safe_error(error) = "Hazel merge got bad idx checkpoint";
    return false;
  }
  addr directory_offset = header_length + final_pst_size;
  addr directory_length = final_dct_size;
  addr directory_count = final_dct_size / hazel_posting_entry_size;
  out->write(hazel_idx_magic.data(), hazel_idx_magic.size());
  write_pod(out, directory_offset);
  write_pod(out, directory_length);
  write_pod(out, directory_count);
  if (out->fail()) {
    safe_error(error) = "Hazel merge failed to write idx blob";
    return false;
  }
  return hazel_copy_checkpoint_bytes(pst_name, final_pst_size, out, error) &&
         hazel_copy_checkpoint_bytes(dct_name, final_dct_size, out, error);
}

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

  static bool merge(const std::vector<std::shared_ptr<HazelTxt>> &txts,
                    std::ostream *out, std::string *error = nullptr);
  addr raw_text_length() const { return raw_text_length_; }

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
  addr target_chunk_size_;
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

struct HazelMergeSidecars {
  std::string mrg;
  std::string pst;
  std::string dct;
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

HazelMergeSidecars hazel_merge_sidecars(const std::string &dst) {
  return {hazel_sidecar_name(dst, "mrg"), hazel_sidecar_name(dst, "pst"),
          hazel_sidecar_name(dst, "dct")};
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

bool hazel_cleanup_prefix_files(const std::string &prefix,
                                std::string *error) {
  std::filesystem::path target(prefix);
  std::filesystem::path directory = target.parent_path();
  if (directory.empty())
    directory = ".";
  std::string base = target.filename().string() + ".";

  std::error_code ec;
  bool exists = std::filesystem::exists(directory, ec);
  if (ec) {
    safe_error(error) = "Hazel merge can't inspect directory: " +
                        directory.string() + ": " + ec.message();
    return false;
  }
  if (!exists)
    return true;

  std::filesystem::directory_iterator it(directory, ec);
  std::filesystem::directory_iterator end;
  if (ec) {
    safe_error(error) = "Hazel merge can't list directory: " +
                        directory.string() + ": " + ec.message();
    return false;
  }
  for (; it != end; it.increment(ec)) {
    if (ec) {
      safe_error(error) = "Hazel merge can't list directory: " +
                          directory.string() + ": " + ec.message();
      return false;
    }
    std::string name = it->path().filename().string();
    if (name.compare(0, base.size(), base) != 0)
      continue;
    std::error_code remove_error;
    std::filesystem::remove(it->path(), remove_error);
    if (remove_error) {
      safe_error(error) = "Hazel merge can't remove file: " +
                          it->path().string() + ": " +
                          remove_error.message();
      return false;
    }
  }
  return true;
}

bool hazel_cleanup_merge_sidecars(const std::string &dst,
                                  const HazelMergeSidecars &sidecars,
                                  std::string *error) {
  if (!hazel_remove_if_exists(sidecars.mrg, error) ||
      !hazel_remove_if_exists(sidecars.pst, error) ||
      !hazel_remove_if_exists(sidecars.dct, error))
    return false;
  return hazel_cleanup_prefix_files(dst, error);
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
    const std::map<std::string, std::string> &last,
    bool sequence_present, addr sequence_start, addr sequence_end,
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
    } else if (previous_end == maxfinity || sequence_start != previous_end + 1) {
      safe_error(error) = "Hazel merge got non-contiguous shards";
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
    if (dna_sequence_present &&
        (dna_sequence_start != sequence_start ||
         dna_sequence_end != sequence_end)) {
      safe_error(error) = "Hazel filename and DNA sequence ranges differ";
      return false;
    }
    activated.push_back(hazel);
  }

  std::string final_name = hazel_default_name(first_start, last_end);
  std::string final_filename = working->make_name(final_name);
  return Hazel::merge(activated, final_filename, nullptr, error);
}

bool Hazel::merge(
    const std::vector<std::shared_ptr<Hazel>> &hazels, const std::string &dst,
    std::shared_ptr<std::map<std::string, std::string>> parameters,
    std::string *error) {
  if (hazels.size() < 2) {
    safe_error(error) = "Hazel merge needs at least two shards";
    return false;
  }
  if (dst == "") {
    safe_error(error) = "Hazel merge got empty destination";
    return false;
  }

  bool exists;
  if (!hazel_path_exists(dst, &exists, error))
    return false;
  HazelMergeSidecars sidecars = hazel_merge_sidecars(dst);
  if (exists)
    return hazel_cleanup_merge_sidecars(dst, sidecars, error);

  if (!hazel_remove_if_exists(sidecars.mrg, error))
    return false;

  std::vector<std::shared_ptr<HazelIdx>> idxs;
  std::vector<std::shared_ptr<HazelTxt>> txts;
  std::vector<addr> text_lengths;
  idxs.reserve(hazels.size());
  txts.reserve(hazels.size());
  text_lengths.reserve(hazels.size());

  std::string normalized;
  bool sequence_present = false;
  addr sequence_start = 0;
  addr sequence_end = 0;
  addr previous_sequence_end = 0;
  for (size_t i = 0; i < hazels.size(); i++) {
    auto hazel = hazels[i];
    if (hazel == nullptr) {
      safe_error(error) = "Hazel merge got null shard";
      return false;
    }
    if (hazel->featurizer_ == nullptr || hazel->idx_ == nullptr ||
        hazel->txt_ == nullptr) {
      safe_error(error) = "Hazel merge got incomplete shard";
      return false;
    }

    auto idx = std::dynamic_pointer_cast<HazelIdx>(hazel->idx_);
    if (idx == nullptr) {
      safe_error(error) = "Hazel merge got non-Hazel idx";
      return false;
    }
    auto txt = std::dynamic_pointer_cast<HazelTxt>(hazel->txt_);
    if (txt == nullptr) {
      safe_error(error) = "Hazel merge got non-Hazel txt";
      return false;
    }

    std::string current;
    if (!normalize_dna_for_activated_hazel_merge(hazel->parameters_, &current,
                                                 error))
      return false;
    if (i == 0)
      normalized = current;
    else if (current != normalized) {
      safe_error(error) = "Hazel merge got incompatible DNA";
      return false;
    }

    bool current_sequence_present;
    addr current_sequence_start;
    addr current_sequence_end;
    if (!hazel_sequence_range(hazel->parameters_, &current_sequence_present,
                              &current_sequence_start,
                              &current_sequence_end, error))
      return false;
    if (i == 0) {
      sequence_present = current_sequence_present;
      if (sequence_present) {
        sequence_start = current_sequence_start;
        sequence_end = current_sequence_end;
        previous_sequence_end = current_sequence_end;
      }
    } else {
      if (current_sequence_present != sequence_present) {
        safe_error(error) = "Hazel merge got inconsistent sequence metadata";
        return false;
      }
      if (sequence_present) {
        if (previous_sequence_end == maxfinity ||
            current_sequence_start != previous_sequence_end + 1) {
          safe_error(error) = "Hazel merge got non-contiguous shards";
          return false;
        }
        sequence_end = current_sequence_end;
        previous_sequence_end = current_sequence_end;
      }
    }

    idxs.push_back(idx);
    txts.push_back(txt);
    text_lengths.push_back(txt->raw_text_length());
  }

  std::string dna;
  if (!merged_activated_hazel_dna(
          hazels.front()->parameters_, hazels.back()->parameters_,
          sequence_present, sequence_start, sequence_end, parameters, &dna,
          error))
    return false;

  addr text_chunk_feature = hazels.front()->featurizer_->featurize(text_chunk_tag);
  HazelMergeOutput output;
  auto remove_temp = [&]() {
    output.out.close();
    hazel_remove_if_exists(sidecars.mrg, nullptr);
  };

  if (!output.open(sidecars.mrg, dna, error)) {
    remove_temp();
    return false;
  }

  output.blobs[0].offset = (addr)output.out.tellp();
  if (!HazelIdx::merge(idxs, text_lengths, text_chunk_feature, sidecars.pst,
                       sidecars.dct, &output.out, error)) {
    remove_temp();
    return false;
  }
  output.blobs[0].length = (addr)output.out.tellp() - output.blobs[0].offset;
  if (output.out.fail() || output.blobs[0].length < 0) {
    safe_error(error) = "Hazel merge failed to write idx blob";
    remove_temp();
    return false;
  }

  output.blobs[1].offset = (addr)output.out.tellp();
  if (!HazelTxt::merge(txts, &output.out, error)) {
    remove_temp();
    return false;
  }
  output.blobs[1].length = (addr)output.out.tellp() - output.blobs[1].offset;
  if (output.out.fail() || output.blobs[1].length < 0) {
    safe_error(error) = "Hazel merge failed to write txt blob";
    remove_temp();
    return false;
  }

  if (!output.close(error)) {
    remove_temp();
    return false;
  }

  if (link(sidecars.mrg.c_str(), dst.c_str()) != 0) {
    if (!hazel_path_exists(dst, &exists, error)) {
      remove_temp();
      return false;
    }
    if (exists) {
      if (!hazel_cleanup_merge_sidecars(dst, sidecars, error)) {
        remove_temp();
        return false;
      }
      return true;
    }
    safe_error(error) = "Hazel merge can't link shard: " + dst;
    remove_temp();
    return false;
  }

  return hazel_cleanup_merge_sidecars(dst, sidecars, error);
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

std::shared_ptr<SimplePosting> Hazel::posting(addr feature) {
  std::shared_ptr<HazelIdx> idx =
      std::static_pointer_cast<HazelIdx>(this->idx());
  return idx->posting(feature);
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
