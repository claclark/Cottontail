#ifndef COTTONTAIL_SRC_OWSLA_H_
#define COTTONTAIL_SRC_OWSLA_H_

#include <cassert>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <ostream>
#include <string>
#include <vector>

#include "src/core.h"
#include "src/simple_posting.h"
#include "src/warren.h"

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

struct OwslaShard {
  OwslaShard() = default;
  OwslaShard(addr start, addr end, const std::string &name)
      : start(start), end(end), name(name){};
  addr start = 0;
  addr end = 0;
  std::string name;
  bool operator<(const OwslaShard &other) const {
    return start < other.start ||
           (start == other.start &&
            (end < other.end || (end == other.end && name < other.name)));
  }
};

struct HazelMergeRecovery {
  OwslaShard target;
  std::vector<OwslaShard> sources;
  bool operator<(const HazelMergeRecovery &other) const {
    return target < other.target;
  }
};

std::string seq2str(addr sequence);
std::string hazel_default_name(addr sequence_start, addr sequence_end);
std::string hazel_blob_dictionary(const std::vector<HazelBlob> &blobs);
std::string owsla_shard_name(const std::string &prefix, addr sequence_start,
                             addr sequence_end);
bool owsla_parse_shard_name(const std::string &name,
                            const std::string &prefix, OwslaShard *shard);
bool owsla_ranges_overlap(const OwslaShard &a, const OwslaShard &b);
bool owsla_range_contains(const OwslaShard &outer, const OwslaShard &inner);

class Owsla : public Warren {
public:
  virtual std::shared_ptr<SimplePosting> posting(addr feature) = 0;
  virtual addr estimated_size() const = 0;

  virtual ~Owsla(){};
  Owsla(const Owsla &) = delete;
  Owsla &operator=(const Owsla &) = delete;
  Owsla(Owsla &&) = delete;
  Owsla &operator=(Owsla &&) = delete;

protected:
  Owsla(std::shared_ptr<Working> working, std::shared_ptr<Featurizer> featurizer,
        std::shared_ptr<Tokenizer> tokenizer, std::shared_ptr<Idx> idx,
        std::shared_ptr<Txt> txt)
      : Warren(working, featurizer, tokenizer, idx, txt){};
};

class OwslaCache final {
public:
  OwslaCache() = default;

  std::shared_ptr<SimplePosting>
  get(addr feature, std::shared_ptr<SimplePostingFactory> posting_factory,
      bool *fill) {
    assert(posting_factory != nullptr);
    assert(fill != nullptr);
    std::lock_guard<std::mutex> lock(lock_);
    auto cached = postings_.find(feature);
    if (cached != postings_.end()) {
      *fill = false;
      return cached->second;
    }
    std::shared_ptr<SimplePosting> posting =
        posting_factory->posting_from_feature(feature, false);
    assert(posting != nullptr);
    postings_[feature] = posting;
    *fill = true;
    return posting;
  }

  OwslaCache(const OwslaCache &) = delete;
  OwslaCache &operator=(const OwslaCache &) = delete;
  OwslaCache(OwslaCache &&) = delete;
  OwslaCache &operator=(OwslaCache &&) = delete;

private:
  std::mutex lock_;
  std::map<addr, std::shared_ptr<SimplePosting>> postings_;
};

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
