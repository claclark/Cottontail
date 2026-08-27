#include <memory>
#include <vector>

#include "gtest/gtest.h"

#include "gcl/materialize.h"
#include "src/core.h"
#include "src/hopper.h"
#include "src/null_idx.h"

namespace {

struct Posting {
  cottontail::addr p;
  cottontail::addr q;
  cottontail::fval v;
};

class CountingSingletonHopper final : public cottontail::Hopper {
public:
  int tau_count = 0;
  int rho_count = 0;
  int uat_count = 0;
  int ohr_count = 0;

private:
  static void set(cottontail::addr p_value, cottontail::addr q_value,
                  cottontail::fval v_value, cottontail::addr *p,
                  cottontail::addr *q, cottontail::fval *v) {
    *p = p_value;
    *q = q_value;
    *v = v_value;
  }

  void tau_(cottontail::addr k, cottontail::addr *p, cottontail::addr *q,
            cottontail::fval *v) final {
    tau_count++;
    if (k == cottontail::minfinity)
      set(cottontail::minfinity, cottontail::minfinity, 0.0, p, q, v);
    else if (k <= 10)
      set(10, 20, 7.0, p, q, v);
    else
      set(cottontail::maxfinity, cottontail::maxfinity, 0.0, p, q, v);
  }

  void rho_(cottontail::addr k, cottontail::addr *p, cottontail::addr *q,
            cottontail::fval *v) final {
    rho_count++;
    if (k == cottontail::minfinity)
      set(cottontail::minfinity, cottontail::minfinity, 0.0, p, q, v);
    else if (k <= 20)
      set(10, 20, 7.0, p, q, v);
    else
      set(cottontail::maxfinity, cottontail::maxfinity, 0.0, p, q, v);
  }

  void uat_(cottontail::addr k, cottontail::addr *p, cottontail::addr *q,
            cottontail::fval *v) final {
    uat_count++;
    if (k == cottontail::maxfinity)
      set(cottontail::maxfinity, cottontail::maxfinity, 0.0, p, q, v);
    else if (k >= 20)
      set(10, 20, 7.0, p, q, v);
    else
      set(cottontail::minfinity, cottontail::minfinity, 0.0, p, q, v);
  }

  void ohr_(cottontail::addr k, cottontail::addr *p, cottontail::addr *q,
            cottontail::fval *v) final {
    ohr_count++;
    if (k == cottontail::maxfinity)
      set(cottontail::maxfinity, cottontail::maxfinity, 0.0, p, q, v);
    else if (k >= 10)
      set(10, 20, 7.0, p, q, v);
    else
      set(cottontail::minfinity, cottontail::minfinity, 0.0, p, q, v);
  }
};

class SequenceHopper final : public cottontail::Hopper {
public:
  explicit SequenceHopper(const std::vector<Posting> &postings)
      : postings_(postings) {}

private:
  static void set(const Posting &posting, cottontail::addr *p,
                  cottontail::addr *q, cottontail::fval *v) {
    *p = posting.p;
    *q = posting.q;
    *v = posting.v;
  }

  static void set_empty(cottontail::addr sentinel, cottontail::addr *p,
                        cottontail::addr *q, cottontail::fval *v) {
    *p = sentinel;
    *q = sentinel;
    *v = 0.0;
  }

  void tau_(cottontail::addr k, cottontail::addr *p, cottontail::addr *q,
            cottontail::fval *v) final {
    if (k == cottontail::minfinity) {
      set_empty(cottontail::minfinity, p, q, v);
      return;
    }
    for (const Posting &posting : postings_)
      if (posting.p >= k) {
        set(posting, p, q, v);
        return;
      }
    set_empty(cottontail::maxfinity, p, q, v);
  }

  void rho_(cottontail::addr k, cottontail::addr *p, cottontail::addr *q,
            cottontail::fval *v) final {
    if (k == cottontail::minfinity) {
      set_empty(cottontail::minfinity, p, q, v);
      return;
    }
    for (const Posting &posting : postings_)
      if (posting.q >= k) {
        set(posting, p, q, v);
        return;
      }
    set_empty(cottontail::maxfinity, p, q, v);
  }

  void uat_(cottontail::addr k, cottontail::addr *p, cottontail::addr *q,
            cottontail::fval *v) final {
    if (k == cottontail::maxfinity) {
      set_empty(cottontail::maxfinity, p, q, v);
      return;
    }
    for (auto posting = postings_.rbegin(); posting != postings_.rend();
         posting++)
      if (posting->q <= k) {
        set(*posting, p, q, v);
        return;
      }
    set_empty(cottontail::minfinity, p, q, v);
  }

  void ohr_(cottontail::addr k, cottontail::addr *p, cottontail::addr *q,
            cottontail::fval *v) final {
    if (k == cottontail::maxfinity) {
      set_empty(cottontail::maxfinity, p, q, v);
      return;
    }
    for (auto posting = postings_.rbegin(); posting != postings_.rend();
         posting++)
      if (posting->p <= k) {
        set(*posting, p, q, v);
        return;
      }
    set_empty(cottontail::minfinity, p, q, v);
  }

  std::vector<Posting> postings_;
};

} // namespace

TEST(Hopper, Empty) {
  cottontail::EmptyHopper hopper;
  cottontail::addr p, q;
  cottontail::fval v;

  hopper.tau(1, &p, &q, &v);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  EXPECT_EQ(v, 0.0);
  hopper.tau(cottontail::maxfinity, &p, &q, &v);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  EXPECT_EQ(v, 0.0);
  hopper.tau(cottontail::minfinity, &p, &q, &v);
  EXPECT_EQ(p, cottontail::minfinity);
  EXPECT_EQ(q, cottontail::minfinity);
  EXPECT_EQ(v, 0.0);

  hopper.rho(10, &p, &q, &v);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  EXPECT_EQ(v, 0.0);
  hopper.rho(cottontail::maxfinity, &p, &q, &v);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  EXPECT_EQ(v, 0.0);
  hopper.rho(cottontail::minfinity, &p, &q, &v);
  EXPECT_EQ(p, cottontail::minfinity);
  EXPECT_EQ(q, cottontail::minfinity);
  EXPECT_EQ(v, 0.0);

  hopper.uat(100, &p, &q, &v);
  EXPECT_EQ(p, cottontail::minfinity);
  EXPECT_EQ(q, cottontail::minfinity);
  EXPECT_EQ(v, 0.0);
  hopper.uat(cottontail::maxfinity, &p, &q, &v);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  EXPECT_EQ(v, 0.0);
  hopper.uat(cottontail::minfinity, &p, &q, &v);
  EXPECT_EQ(p, cottontail::minfinity);
  EXPECT_EQ(q, cottontail::minfinity);
  EXPECT_EQ(v, 0.0);

  hopper.ohr(1000, &p, &q, &v);
  EXPECT_EQ(p, cottontail::minfinity);
  EXPECT_EQ(q, cottontail::minfinity);
  EXPECT_EQ(v, 0.0);
  hopper.ohr(cottontail::maxfinity, &p, &q, &v);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  EXPECT_EQ(v, 0.0);
  hopper.ohr(cottontail::minfinity, &p, &q, &v);
  EXPECT_EQ(p, cottontail::minfinity);
  EXPECT_EQ(q, cottontail::minfinity);
  EXPECT_EQ(v, 0.0);
}

TEST(Hopper, Singleton) {
  cottontail::SingletonHopper hopper(8, 16, 1.0);
  cottontail::addr p, q;
  cottontail::fval v;

  hopper.tau(cottontail::minfinity, &p, &q, &v);
  EXPECT_EQ(p, cottontail::minfinity);
  EXPECT_EQ(q, cottontail::minfinity);
  EXPECT_EQ(v, 0.0);
  hopper.tau(4, &p, &q, &v);
  EXPECT_EQ(p, 8);
  EXPECT_EQ(q, 16);
  EXPECT_EQ(v, 1.0);
  hopper.tau(32, &p, &q, &v);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  EXPECT_EQ(v, 0.0);

  hopper.rho(cottontail::minfinity, &p, &q, &v);
  EXPECT_EQ(p, cottontail::minfinity);
  EXPECT_EQ(q, cottontail::minfinity);
  EXPECT_EQ(v, 0.0);
  hopper.rho(12, &p, &q, &v);
  EXPECT_EQ(p, 8);
  EXPECT_EQ(q, 16);
  EXPECT_EQ(v, 1.0);
  hopper.rho(32, &p, &q, &v);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  EXPECT_EQ(v, 0.0);

  hopper.uat(cottontail::maxfinity, &p, &q, &v);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  EXPECT_EQ(v, 0.0);
  hopper.uat(32, &p, &q, &v);
  EXPECT_EQ(p, 8);
  EXPECT_EQ(q, 16);
  EXPECT_EQ(v, 1.0);
  hopper.uat(12, &p, &q, &v);
  EXPECT_EQ(p, cottontail::minfinity);
  EXPECT_EQ(q, cottontail::minfinity);

  hopper.ohr(cottontail::maxfinity, &p, &q, &v);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  hopper.ohr(12, &p, &q, &v);
  EXPECT_EQ(p, 8);
  EXPECT_EQ(q, 16);
  EXPECT_EQ(v, 1.0);
  hopper.uat(4, &p, &q, &v);
  EXPECT_EQ(p, cottontail::minfinity);
  EXPECT_EQ(q, cottontail::minfinity);
}

TEST(Hopper, FixedWidth) {
  cottontail::FixedWidthHopper hopper(8);
  cottontail::addr p, q;
  cottontail::fval v;

  hopper.tau(0, &p, &q, &v);
  EXPECT_EQ(p, 0);
  EXPECT_EQ(q, 7);
  EXPECT_EQ(v, 0.0);
  hopper.tau(cottontail::maxfinity, &p, &q, &v);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  EXPECT_EQ(v, 0.0);
  hopper.tau(cottontail::minfinity, &p, &q, &v);
  EXPECT_EQ(p, cottontail::minfinity);
  EXPECT_EQ(q, cottontail::minfinity);
  EXPECT_EQ(v, 0.0);

  hopper.rho(0, &p, &q, &v);
  EXPECT_EQ(p, -7);
  EXPECT_EQ(q, 0);
  EXPECT_EQ(v, 0.0);
  hopper.rho(cottontail::maxfinity, &p, &q, &v);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  EXPECT_EQ(v, 0.0);
  hopper.rho(cottontail::minfinity, &p, &q, &v);
  EXPECT_EQ(p, cottontail::minfinity);
  EXPECT_EQ(q, cottontail::minfinity);
  EXPECT_EQ(v, 0.0);

  hopper.uat(0, &p, &q, &v);
  EXPECT_EQ(p, -7);
  EXPECT_EQ(q, 0);
  EXPECT_EQ(v, 0.0);
  hopper.uat(cottontail::maxfinity, &p, &q, &v);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  EXPECT_EQ(v, 0.0);
  hopper.uat(cottontail::minfinity, &p, &q, &v);
  EXPECT_EQ(p, cottontail::minfinity);
  EXPECT_EQ(q, cottontail::minfinity);
  EXPECT_EQ(v, 0.0);

  hopper.ohr(0, &p, &q, &v);
  EXPECT_EQ(p, 0);
  EXPECT_EQ(q, 7);
  EXPECT_EQ(v, 0.0);
  hopper.ohr(cottontail::maxfinity, &p, &q, &v);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  EXPECT_EQ(v, 0.0);
  hopper.ohr(cottontail::minfinity, &p, &q, &v);
  EXPECT_EQ(p, cottontail::minfinity);
  EXPECT_EQ(q, cottontail::minfinity);
  EXPECT_EQ(v, 0.0);
}

TEST(Hopper, Universal) {
  cottontail::UniversalHopper hopper;
  cottontail::addr p, q;
  cottontail::fval v;

  hopper.tau(37, &p, &q, &v);
  EXPECT_EQ(p, 37);
  EXPECT_EQ(q, 37);
  EXPECT_EQ(v, 0.0);
  hopper.rho(37, &p, &q, &v);
  EXPECT_EQ(p, 37);
  EXPECT_EQ(q, 37);
  hopper.uat(37, &p, &q, &v);
  EXPECT_EQ(p, 37);
  EXPECT_EQ(q, 37);
  hopper.ohr(37, &p, &q, &v);
  EXPECT_EQ(p, 37);
  EXPECT_EQ(q, 37);

  hopper.tau(cottontail::minfinity, &p, &q);
  EXPECT_EQ(p, cottontail::minfinity);
  EXPECT_EQ(q, cottontail::minfinity);
  hopper.ohr(cottontail::maxfinity, &p, &q);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
}

TEST(Idx, UniversalFeature) {
  std::shared_ptr<cottontail::Idx> idx = cottontail::NullIdx::make("");
  ASSERT_NE(idx, nullptr);
  EXPECT_EQ(idx->count(cottontail::universal_feature), 0);
  std::unique_ptr<cottontail::Hopper> hopper =
      idx->hopper(cottontail::universal_feature);
  ASSERT_NE(hopper, nullptr);
  cottontail::addr p, q;
  hopper->tau(42, &p, &q);
  EXPECT_EQ(p, 42);
  EXPECT_EQ(q, 42);
}

TEST(Hopper, MemoizationReusesEquivalentForwardQueries) {
  CountingSingletonHopper hopper;
  cottontail::addr p, q;
  cottontail::fval v;

  hopper.tau(5, &p, &q, &v);
  EXPECT_EQ(hopper.tau_count, 1);
  EXPECT_EQ(p, 10);
  EXPECT_EQ(q, 20);
  EXPECT_EQ(v, 7.0);
  hopper.tau(7, &p, &q, &v);
  EXPECT_EQ(hopper.tau_count, 1);
  EXPECT_EQ(p, 10);
  EXPECT_EQ(q, 20);
  EXPECT_EQ(v, 7.0);
  hopper.tau(10, &p, &q, &v);
  EXPECT_EQ(hopper.tau_count, 1);
  hopper.tau(11, &p, &q, &v);
  EXPECT_EQ(hopper.tau_count, 2);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  EXPECT_EQ(v, 0.0);

  hopper.rho(12, &p, &q, &v);
  EXPECT_EQ(hopper.rho_count, 1);
  EXPECT_EQ(p, 10);
  EXPECT_EQ(q, 20);
  EXPECT_EQ(v, 7.0);
  hopper.rho(18, &p, &q, &v);
  EXPECT_EQ(hopper.rho_count, 1);
  EXPECT_EQ(p, 10);
  EXPECT_EQ(q, 20);
  EXPECT_EQ(v, 7.0);
  hopper.rho(20, &p, &q, &v);
  EXPECT_EQ(hopper.rho_count, 1);
  hopper.rho(21, &p, &q, &v);
  EXPECT_EQ(hopper.rho_count, 2);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  EXPECT_EQ(v, 0.0);
}

TEST(Hopper, MemoizationReusesEquivalentBackwardQueries) {
  CountingSingletonHopper hopper;
  cottontail::addr p, q;
  cottontail::fval v;

  hopper.uat(30, &p, &q, &v);
  EXPECT_EQ(hopper.uat_count, 1);
  EXPECT_EQ(p, 10);
  EXPECT_EQ(q, 20);
  EXPECT_EQ(v, 7.0);
  hopper.uat(25, &p, &q, &v);
  EXPECT_EQ(hopper.uat_count, 1);
  EXPECT_EQ(p, 10);
  EXPECT_EQ(q, 20);
  EXPECT_EQ(v, 7.0);
  hopper.uat(20, &p, &q, &v);
  EXPECT_EQ(hopper.uat_count, 1);
  hopper.uat(19, &p, &q, &v);
  EXPECT_EQ(hopper.uat_count, 2);
  EXPECT_EQ(p, cottontail::minfinity);
  EXPECT_EQ(q, cottontail::minfinity);
  EXPECT_EQ(v, 0.0);

  hopper.ohr(30, &p, &q, &v);
  EXPECT_EQ(hopper.ohr_count, 1);
  EXPECT_EQ(p, 10);
  EXPECT_EQ(q, 20);
  EXPECT_EQ(v, 7.0);
  hopper.ohr(15, &p, &q, &v);
  EXPECT_EQ(hopper.ohr_count, 1);
  EXPECT_EQ(p, 10);
  EXPECT_EQ(q, 20);
  EXPECT_EQ(v, 7.0);
  hopper.ohr(10, &p, &q, &v);
  EXPECT_EQ(hopper.ohr_count, 1);
  hopper.ohr(9, &p, &q, &v);
  EXPECT_EQ(hopper.ohr_count, 2);
  EXPECT_EQ(p, cottontail::minfinity);
  EXPECT_EQ(q, cottontail::minfinity);
  EXPECT_EQ(v, 0.0);
}

TEST(Materialize, PreservesMaterializedTriples) {
  std::vector<Posting> postings = {{10, 12, 1.5}, {20, 20, 0.0},
                                   {30, 35, 2.25}};
  cottontail::gcl::Materialize hopper(
      std::make_unique<SequenceHopper>(postings));
  cottontail::addr p, q;
  cottontail::fval v;

  hopper.tau(0, &p, &q, &v);
  EXPECT_EQ(p, 10);
  EXPECT_EQ(q, 12);
  EXPECT_EQ(v, 1.5);
  hopper.tau(11, &p, &q, &v);
  EXPECT_EQ(p, 20);
  EXPECT_EQ(q, 20);
  EXPECT_EQ(v, 0.0);
  hopper.tau(21, &p, &q, &v);
  EXPECT_EQ(p, 30);
  EXPECT_EQ(q, 35);
  EXPECT_EQ(v, 2.25);

  hopper.rho(13, &p, &q, &v);
  EXPECT_EQ(p, 20);
  EXPECT_EQ(q, 20);
  EXPECT_EQ(v, 0.0);
  hopper.rho(21, &p, &q, &v);
  EXPECT_EQ(p, 30);
  EXPECT_EQ(q, 35);
  EXPECT_EQ(v, 2.25);

  hopper.uat(34, &p, &q, &v);
  EXPECT_EQ(p, 20);
  EXPECT_EQ(q, 20);
  EXPECT_EQ(v, 0.0);
  hopper.uat(35, &p, &q, &v);
  EXPECT_EQ(p, 30);
  EXPECT_EQ(q, 35);
  EXPECT_EQ(v, 2.25);

  hopper.ohr(25, &p, &q, &v);
  EXPECT_EQ(p, 20);
  EXPECT_EQ(q, 20);
  EXPECT_EQ(v, 0.0);
  hopper.ohr(30, &p, &q, &v);
  EXPECT_EQ(p, 30);
  EXPECT_EQ(q, 35);
  EXPECT_EQ(v, 2.25);
}
