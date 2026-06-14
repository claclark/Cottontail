#ifndef COTTONTAIL_SRC_SIMPLE_POSTING_H_
#define COTTONTAIL_SRC_SIMPLE_POSTING_H_

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "src/cache_gate.h"
#include "src/compressor.h"
#include "src/core.h"
#include "src/simple.h"

namespace cottontail {

struct CacheRecord;

class SimplePosting final {
public:
  void append(std::shared_ptr<SimplePosting> more);
  void write(std::ostream *f);
  bool get(size_t index, addr *p, addr *q, fval *v);
  void push(addr p, addr q, fval v);
  addr feature() { return feature_; };
  bool invariants(std::string *error = nullptr);
  bool operator==(const SimplePosting &other);
  inline size_t size() { return postings_.size(); }

  SimplePosting(const SimplePosting &) = delete;
  SimplePosting &operator=(const SimplePosting &) = delete;
  SimplePosting(SimplePosting &&) = delete;
  SimplePosting &operator=(SimplePosting &&) = delete;
  inline void wait() { gate_.wait(); };
  inline void release() { gate_.open(); }

private:
  SimplePosting(std::shared_ptr<Compressor> posting_compressor,
                std::shared_ptr<Compressor> fvalue_compressor)
      : gate_(true), posting_compressor_(posting_compressor),
        fvalue_compressor_(fvalue_compressor){};
  SimplePosting(std::shared_ptr<Compressor> posting_compressor,
                std::shared_ptr<Compressor> fvalue_compressor, bool open)
      : gate_(open), posting_compressor_(posting_compressor),
        fvalue_compressor_(fvalue_compressor){};
  addr feature_;
  CacheGate gate_;
  std::vector<addr> postings_;
  std::vector<addr> qostings_;
  std::vector<fval> fostings_;
  std::shared_ptr<Compressor> posting_compressor_;
  std::shared_ptr<Compressor> fvalue_compressor_;

  friend class ArrayHopper;
  friend class SimplePostingFactory;
};

class SimplePostingFactory final {
public:
  SimplePostingFactory(std::shared_ptr<Compressor> posting_compressor,
                       std::shared_ptr<Compressor> fvalue_compressor)
      : posting_compressor_(posting_compressor),
        fvalue_compressor_(fvalue_compressor){};
  static std::shared_ptr<SimplePostingFactory>
  make(std::shared_ptr<Compressor> posting_compressor,
       std::shared_ptr<Compressor> fvalue_compressor) {
    return std::make_shared<SimplePostingFactory>(posting_compressor,
                                                  fvalue_compressor);
  };
  std::shared_ptr<SimplePosting>
  posting_from_tokens(std::vector<TokRecord>::iterator *start,
                      const std::vector<TokRecord>::iterator &end);
  std::shared_ptr<SimplePosting>
  posting_from_annotations(std::vector<Annotation>::iterator *start,
                           const std::vector<Annotation>::iterator &end);
  std::shared_ptr<SimplePosting> posting_from_feature(addr feature,
                                                      bool open = true);
  std::shared_ptr<SimplePosting>
  posting_from_compressed_blob(const char *data, addr length,
                               std::string *error = nullptr);
  bool cache_entry_from_compressed_blob(std::shared_ptr<CacheRecord> cache_line,
                                        const char *data, addr length,
                                        std::string *error = nullptr);
  std::shared_ptr<SimplePosting> posting_from_file(std::fstream *f);
  std::shared_ptr<SimplePosting> posting_from_merge(
      const std::vector<std::shared_ptr<SimplePosting>> &postings);
  std::shared_ptr<SimplePosting> posting_from_merge(
      const std::vector<std::shared_ptr<SimplePosting>> &postings,
      std::shared_ptr<SimplePosting> exclude);

  SimplePostingFactory(const SimplePostingFactory &) = delete;
  SimplePostingFactory &operator=(const SimplePostingFactory &) = delete;
  SimplePostingFactory(SimplePostingFactory &&) = delete;
  SimplePostingFactory &operator=(SimplePostingFactory &&) = delete;

private:
  std::shared_ptr<Compressor> posting_compressor_;
  std::shared_ptr<Compressor> fvalue_compressor_;
};
} // namespace cottontail
#endif // COTTONTAIL_SRC_SIMPLE_POSTING_H_
