#ifndef COTTONTAIL_SRC_ARRAY_HOPPER_H_
#define COTTONTAIL_SRC_ARRAY_HOPPER_H_

#include <cassert>
#include <memory>

#include "src/core.h"
#include "src/hopper.h"
#include "src/hints.h"

namespace cottontail {

struct CacheRecord {
  addr n;
  std::shared_ptr<addr> postings;
  std::shared_ptr<addr> qostings;
  std::shared_ptr<fval> fostings;
  bool ready;
  std::mutex lock;
  std::condition_variable condition;
  inline void wait() {
    if (!ready) {
      std::unique_lock<std::mutex> l(lock);
      do {
        condition.wait(l, [&] { return ready; });
      } while (!ready);
    }
    condition.notify_all();
  };
};

class ArrayHopper final : public Hopper {
public:
  ArrayHopper(addr n, std::shared_ptr<addr> postings,
              std::shared_ptr<addr> qostings,
              std::shared_ptr<fval> fostings = nullptr)
      : n_(n), p_storage_(postings), q_storage_(qostings), f_storage_(fostings),
        cache_line_(nullptr), ready_(true) {
    assert(postings != nullptr && qostings != nullptr && n_ >= 2);
    postings_ = p_storage_.get();
    qostings_ = q_storage_.get();
    if (f_storage_ == nullptr)
      fostings_ = nullptr;
    else
      fostings_ = f_storage_.get();
  };
  static std::unique_ptr<Hopper>
  make(addr n, std::shared_ptr<addr> postings, std::shared_ptr<addr> qostings,
       std::shared_ptr<fval> fostings = nullptr) {
    return std::make_unique<ArrayHopper>(n, postings, qostings, fostings);
  };
  ArrayHopper(std::shared_ptr<CacheRecord> cache_line)
      : n_(cache_line->n), p_storage_(nullptr), q_storage_(nullptr),
        f_storage_(nullptr), cache_line_(cache_line), ready_(false) {
    assert(n_ >= 2);
  };
  static std::unique_ptr<Hopper> make(std::shared_ptr<CacheRecord> cache_line) {
    return std::make_unique<ArrayHopper>(cache_line);
  };

  virtual ~ArrayHopper(){};
  ArrayHopper(const ArrayHopper &) = delete;
  ArrayHopper &operator=(const ArrayHopper &) = delete;
  ArrayHopper(ArrayHopper &&) = delete;
  ArrayHopper &operator=(ArrayHopper &&) = delete;

private:
  addr L_(addr k) final;
  addr R_(addr k) final;
  void tau_(addr k, addr *p, addr *q, fval *v) final;
  void rho_(addr k, addr *p, addr *q, fval *v) final;
  void uat_(addr k, addr *p, addr *q, fval *v) final;
  void ohr_(addr k, addr *p, addr *q, fval *v) final;
  inline void wait() {
    if (UNLIKELY(!ready_)) {
      cache_line_->wait();
      assert(cache_line_->postings != nullptr &&
             cache_line_->qostings != nullptr);
      postings_ = cache_line_->postings.get();
      qostings_ = cache_line_->qostings.get();
      if (cache_line_->fostings == nullptr)
        fostings_ = nullptr;
      else
        fostings_ = cache_line_->fostings.get();
      ready_ = true;
    }
  };

  addr n_;
  std::shared_ptr<addr> p_storage_;
  std::shared_ptr<addr> q_storage_;
  std::shared_ptr<fval> f_storage_;
  std::shared_ptr<CacheRecord> cache_line_;
  addr current_ = 0;
  const addr *postings_;
  const addr *qostings_;
  const fval *fostings_;
  bool ready_;
};

} // namespace cottontail
#endif // COTTONTAIL_SRC_ARRAY_HOPPER_H_
