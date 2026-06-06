#ifndef COTTONTAIL_SRC_ARRAY_HOPPER_H_
#define COTTONTAIL_SRC_ARRAY_HOPPER_H_

#include <cassert>
#include <memory>

#include "src/cache_gate.h"
#include "src/core.h"
#include "src/hints.h"
#include "src/hopper.h"
#include "src/simple_posting.h"

namespace cottontail {

struct CacheRecord {
  addr n;
  std::shared_ptr<addr> postings;
  std::shared_ptr<addr> qostings;
  std::shared_ptr<fval> fostings;
  CacheGate gate_{false};
  inline void wait() { gate_.wait(); };
  inline void release() { gate_.open(); }
};

class ArrayHopper final : public Hopper {
public:
  static std::unique_ptr<Hopper>
  make(addr n, std::shared_ptr<addr> postings, std::shared_ptr<addr> qostings,
       std::shared_ptr<fval> fostings = nullptr) {
    return std::unique_ptr<Hopper>(
        new ArrayHopper(n, postings, qostings, fostings));
  };
  static std::unique_ptr<Hopper> make(std::shared_ptr<CacheRecord> cache_line) {
    return std::unique_ptr<Hopper>(new ArrayHopper(cache_line));
  };
  static std::unique_ptr<Hopper>
  make(std::shared_ptr<SimplePosting> posting_storage) {
    return std::unique_ptr<Hopper>(new ArrayHopper(posting_storage));
  }

  virtual ~ArrayHopper(){};
  ArrayHopper(const ArrayHopper &) = delete;
  ArrayHopper &operator=(const ArrayHopper &) = delete;
  ArrayHopper(ArrayHopper &&) = delete;
  ArrayHopper &operator=(ArrayHopper &&) = delete;

private:
  ArrayHopper(addr n, std::shared_ptr<addr> postings,
              std::shared_ptr<addr> qostings,
              std::shared_ptr<fval> fostings = nullptr)
      : n_(n), p_storage_(postings), q_storage_(qostings), f_storage_(fostings),
        cache_line_(nullptr), posting_storage_(nullptr),
        postings_(p_storage_.get()), qostings_(q_storage_.get()),
        fostings_(f_storage_.get()), ready_(true) {
    assert(postings != nullptr && qostings != nullptr && n_ > 0);
  };
  ArrayHopper(std::shared_ptr<CacheRecord> cache_line)
      : n_(0), p_storage_(nullptr), q_storage_(nullptr), f_storage_(nullptr),
        cache_line_(cache_line), posting_storage_(nullptr), postings_(nullptr),
        qostings_(nullptr), fostings_(nullptr), ready_(false) {
    assert(cache_line_ != nullptr);
  };
  ArrayHopper(std::shared_ptr<SimplePosting> posting_storage)
      : n_(0), p_storage_(nullptr), q_storage_(nullptr), f_storage_(nullptr),
        cache_line_(nullptr), posting_storage_(posting_storage),
        postings_(nullptr), qostings_(nullptr), fostings_(nullptr),
        ready_(false) {
    assert(posting_storage != nullptr);
  }
  addr L_(addr k) final;
  addr R_(addr k) final;
  void tau_(addr k, addr *p, addr *q, fval *v) final;
  void rho_(addr k, addr *p, addr *q, fval *v) final;
  void uat_(addr k, addr *p, addr *q, fval *v) final;
  void ohr_(addr k, addr *p, addr *q, fval *v) final;
  void bind();
  inline void wait() {
    // AI - Not a race condition because object is thread local.
    if (UNLIKELY(!ready_)) {
      bind();
      ready_ = true;
    }
  };
  addr n_;
  std::shared_ptr<addr> p_storage_;
  std::shared_ptr<addr> q_storage_;
  std::shared_ptr<fval> f_storage_;
  std::shared_ptr<CacheRecord> cache_line_;
  std::shared_ptr<SimplePosting> posting_storage_;
  addr current_ = 0;
  const addr *postings_;
  const addr *qostings_;
  const fval *fostings_;
  bool ready_;
};

} // namespace cottontail
#endif // COTTONTAIL_SRC_ARRAY_HOPPER_H_
