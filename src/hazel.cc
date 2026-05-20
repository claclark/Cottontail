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
  addr raw_end;
  addr compressed_end;
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
    return "";
  };
  std::string raw_(addr p, addr q) final { return translate(p, q); };
  addr tokens_() final { return 0; };
  bool range_(addr *p, addr *q) final {
    *p = maxfinity;
    *q = maxfinity;
    return false;
  }

  std::string therecipe_;
  std::shared_ptr<ReadGate> read_gate_;
  std::shared_ptr<Tokenizer> tokenizer_;
  std::unique_ptr<Hopper> hopper_;
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
