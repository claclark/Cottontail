#ifndef COTTONTAIL_SRC_ARRAY_HOPPER_H_
#define COTTONTAIL_SRC_ARRAY_HOPPER_H_

#include <cassert>
#include <memory>

#include "src/core.h"
#include "src/hopper.h"

namespace cottontail {

class ArrayHopper final : public Hopper {
public:
  ArrayHopper(addr n, std::shared_ptr<addr> postings,
              std::shared_ptr<addr> qostings,
              std::shared_ptr<fval> fostings = nullptr,
              std::shared_ptr<CacheRecord> cache_line = nullptr)
      : n_(n), p_storage_(postings), q_storage_(qostings), f_storage_(fostings),
        cache_line_(cache_line) {
    assert(postings != nullptr && qostings != nullptr && n_ >= 2);
    current_ = 0;
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
    return std::make_unique<ArrayHopper>(n, postings, qostings, fostings,
                                         nullptr);
  };
  static std::unique_ptr<Hopper> make(std::shared_ptr<CacheRecord> cache_line) {
    return std::make_unique<ArrayHopper>(cache_line->n, cache_line->postings,
                                         cache_line->qostings,
                                         cache_line->fostings, cache_line);
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

  addr n_;
  std::shared_ptr<addr> p_storage_;
  std::shared_ptr<addr> q_storage_;
  std::shared_ptr<fval> f_storage_;
  std::shared_ptr<CacheRecord> cache_line_;
  addr current_ = 0;
  const addr *postings_;
  const addr *qostings_;
  const fval *fostings_;
};

} // namespace cottontail
#endif // COTTONTAIL_SRC_ARRAY_HOPPER_H_
