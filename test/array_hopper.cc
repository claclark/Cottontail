#include <memory>
#include <string>

#include "gtest/gtest.h"

#include "src/array_hopper.h"
#include "src/core.h"

namespace {

std::unique_ptr<cottontail::Hopper>
singleton_array_hopper(cottontail::addr p = 1000, cottontail::addr q = 1000,
                       cottontail::fval v = 1000.0) {
  constexpr cottontail::addr n = 1;
  std::shared_ptr<cottontail::addr> postings =
      cottontail::shared_array<cottontail::addr>(n);
  std::shared_ptr<cottontail::addr> qostings =
      cottontail::shared_array<cottontail::addr>(n);
  std::shared_ptr<cottontail::fval> fostings =
      cottontail::shared_array<cottontail::fval>(n);
  postings.get()[0] = p;
  qostings.get()[0] = q;
  fostings.get()[0] = v;
  return cottontail::ArrayHopper::make(n, postings, qostings, fostings);
}

void expect_interval(cottontail::addr p, cottontail::addr q, cottontail::fval v,
                     cottontail::addr expected_p,
                     cottontail::addr expected_q,
                     cottontail::fval expected_v) {
  EXPECT_EQ(p, expected_p);
  EXPECT_EQ(q, expected_q);
  EXPECT_DOUBLE_EQ(v, expected_v);
}

} // namespace

TEST(ArrayHopper, Basic) {
  std::string error;
  cottontail::addr n = 4;
  cottontail::addr px[] = {1, 10, 20, 100};
  std::shared_ptr<cottontail::addr> postings =
      cottontail::shared_array<cottontail::addr>(n);
  ASSERT_NE(postings, nullptr);
  for (int i = 0; i < n; i++)
    postings.get()[i] = px[i];
  cottontail::addr qx[] = {11, 15, 20, 110};
  std::shared_ptr<cottontail::addr> qostings =
      cottontail::shared_array<cottontail::addr>(n);
  ASSERT_NE(qostings, nullptr);
  for (int i = 0; i < n; i++)
    qostings.get()[i] = qx[i];
  cottontail::fval fx[] = {1.0, 2.0, 3.0, 4.0};
  std::shared_ptr<cottontail::fval> fostings =
      cottontail::shared_array<cottontail::fval>(n);
  ASSERT_NE(fostings, nullptr);
  for (int i = 0; i < n; i++)
    fostings.get()[i] = fx[i];
  std::unique_ptr<cottontail::Hopper> hopper =
      cottontail::ArrayHopper::make(n, postings, qostings, fostings);
  cottontail::addr p, q;
  cottontail::fval v;

  hopper->ohr(cottontail::minfinity, &p, &q, &v);
  EXPECT_EQ(p, cottontail::minfinity);
  EXPECT_EQ(q, cottontail::minfinity);
  EXPECT_EQ(v, 0.0);
  hopper->ohr(-99, &p, &q, &v);
  EXPECT_EQ(p, cottontail::minfinity);
  EXPECT_EQ(q, cottontail::minfinity);
  EXPECT_EQ(v, 0.0);
  hopper->ohr(1, &p, &q, &v);
  EXPECT_EQ(p, 1);
  EXPECT_EQ(q, 11);
  EXPECT_EQ(v, 1.0);
  hopper->ohr(12, &p, &q, &v);
  EXPECT_EQ(p, 10);
  EXPECT_EQ(q, 15);
  EXPECT_EQ(v, 2.0);
  hopper->ohr(20, &p, &q, &v);
  EXPECT_EQ(p, 20);
  EXPECT_EQ(q, 20);
  EXPECT_EQ(v, 3.0);
  hopper->ohr(99, &p, &q, &v);
  EXPECT_EQ(p, 20);
  EXPECT_EQ(q, 20);
  EXPECT_EQ(v, 3.0);
  hopper->ohr(1000, &p, &q, &v);
  EXPECT_EQ(p, 100);
  EXPECT_EQ(q, 110);
  EXPECT_EQ(v, 4.0);
  hopper->ohr(cottontail::maxfinity, &p, &q, &v);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  EXPECT_EQ(v, 0.0);

  hopper->uat(cottontail::minfinity, &p, &q, &v);
  EXPECT_EQ(p, cottontail::minfinity);
  EXPECT_EQ(q, cottontail::minfinity);
  EXPECT_EQ(v, 0.0);
  hopper->uat(-99, &p, &q, &v);
  EXPECT_EQ(p, cottontail::minfinity);
  EXPECT_EQ(q, cottontail::minfinity);
  EXPECT_EQ(v, 0.0);
  hopper->uat(1, &p, &q, &v);
  EXPECT_EQ(p, cottontail::minfinity);
  EXPECT_EQ(q, cottontail::minfinity);
  EXPECT_EQ(v, 0.0);
  hopper->uat(12, &p, &q, &v);
  EXPECT_EQ(p, 1);
  EXPECT_EQ(q, 11);
  EXPECT_EQ(v, 1.0);
  hopper->uat(16, &p, &q, &v);
  EXPECT_EQ(p, 10);
  EXPECT_EQ(q, 15);
  EXPECT_EQ(v, 2.0);
  hopper->uat(20, &p, &q, &v);
  EXPECT_EQ(p, 20);
  EXPECT_EQ(q, 20);
  EXPECT_EQ(v, 3.0);
  hopper->uat(1000, &p, &q, &v);
  EXPECT_EQ(p, 100);
  EXPECT_EQ(q, 110);
  EXPECT_EQ(v, 4.0);
  hopper->uat(cottontail::maxfinity, &p, &q, &v);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  EXPECT_EQ(v, 0.0);

  hopper->rho(cottontail::minfinity, &p, &q, &v);
  EXPECT_EQ(p, cottontail::minfinity);
  EXPECT_EQ(q, cottontail::minfinity);
  EXPECT_EQ(v, 0.0);
  hopper->rho(-99, &p, &q, &v);
  EXPECT_EQ(p, 1);
  EXPECT_EQ(q, 11);
  EXPECT_EQ(v, 1.0);
  hopper->rho(1, &p, &q, &v);
  EXPECT_EQ(p, 1);
  EXPECT_EQ(q, 11);
  EXPECT_EQ(v, 1.0);
  hopper->rho(2, &p, &q, &v);
  EXPECT_EQ(p, 1);
  EXPECT_EQ(q, 11);
  EXPECT_EQ(v, 1.0);
  hopper->rho(12, &p, &q, &v);
  EXPECT_EQ(p, 10);
  EXPECT_EQ(q, 15);
  EXPECT_EQ(v, 2.0);
  hopper->rho(100, &p, &q, &v);
  EXPECT_EQ(p, 100);
  EXPECT_EQ(q, 110);
  EXPECT_EQ(v, 4.0);
  hopper->rho(1000, &p, &q, &v);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  EXPECT_EQ(v, 0.0);
  hopper->rho(cottontail::maxfinity, &p, &q, &v);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  EXPECT_EQ(v, 0.0);

  hopper->tau(cottontail::minfinity, &p, &q, &v);
  EXPECT_EQ(p, cottontail::minfinity);
  EXPECT_EQ(q, cottontail::minfinity);
  EXPECT_EQ(v, 0.0);
  hopper->tau(-99, &p, &q, &v);
  EXPECT_EQ(p, 1);
  EXPECT_EQ(q, 11);
  EXPECT_EQ(v, 1.0);
  hopper->tau(1, &p, &q, &v);
  EXPECT_EQ(p, 1);
  EXPECT_EQ(q, 11);
  EXPECT_EQ(v, 1.0);
  hopper->tau(2, &p, &q, &v);
  EXPECT_EQ(p, 10);
  EXPECT_EQ(q, 15);
  EXPECT_EQ(v, 2.0);
  hopper->tau(21, &p, &q, &v);
  EXPECT_EQ(p, 100);
  EXPECT_EQ(q, 110);
  EXPECT_EQ(v, 4.0);
  hopper->tau(100, &p, &q, &v);
  EXPECT_EQ(p, 100);
  EXPECT_EQ(q, 110);
  EXPECT_EQ(v, 4.0);
  hopper->tau(1000, &p, &q, &v);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  EXPECT_EQ(v, 0.0);
  hopper->tau(cottontail::maxfinity, &p, &q, &v);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  EXPECT_EQ(v, 0.0);
}

TEST(ArrayHopper, Singleton) {
  cottontail::addr p, q;
  cottontail::fval v;

  std::unique_ptr<cottontail::Hopper> hopper = singleton_array_hopper();
  hopper->tau(1, &p, &q, &v);
  hopper->tau(cottontail::minfinity, &p, &q, &v);
  expect_interval(p, q, v, cottontail::minfinity, cottontail::minfinity, 0.0);
  hopper->tau(1, &p, &q, &v);
  expect_interval(p, q, v, 1000, 1000, 1000.0);
  hopper->tau(1000, &p, &q, &v);
  expect_interval(p, q, v, 1000, 1000, 1000.0);
  hopper->tau(2000, &p, &q, &v);
  expect_interval(p, q, v, cottontail::maxfinity, cottontail::maxfinity, 0.0);
  hopper->tau(cottontail::maxfinity, &p, &q, &v);
  expect_interval(p, q, v, cottontail::maxfinity, cottontail::maxfinity, 0.0);

  hopper = singleton_array_hopper();
  hopper->rho(1, &p, &q, &v);
  hopper->rho(cottontail::minfinity, &p, &q, &v);
  expect_interval(p, q, v, cottontail::minfinity, cottontail::minfinity, 0.0);
  hopper->rho(1, &p, &q, &v);
  expect_interval(p, q, v, 1000, 1000, 1000.0);
  hopper->rho(1000, &p, &q, &v);
  expect_interval(p, q, v, 1000, 1000, 1000.0);
  hopper->rho(2000, &p, &q, &v);
  expect_interval(p, q, v, cottontail::maxfinity, cottontail::maxfinity, 0.0);
  hopper->rho(cottontail::maxfinity, &p, &q, &v);
  expect_interval(p, q, v, cottontail::maxfinity, cottontail::maxfinity, 0.0);

  hopper = singleton_array_hopper();
  hopper->uat(cottontail::minfinity, &p, &q, &v);
  expect_interval(p, q, v, cottontail::minfinity, cottontail::minfinity, 0.0);
  hopper->uat(1, &p, &q, &v);
  expect_interval(p, q, v, cottontail::minfinity, cottontail::minfinity, 0.0);
  hopper->uat(1000, &p, &q, &v);
  expect_interval(p, q, v, 1000, 1000, 1000.0);
  hopper->uat(2000, &p, &q, &v);
  expect_interval(p, q, v, 1000, 1000, 1000.0);
  hopper->uat(cottontail::maxfinity, &p, &q, &v);
  expect_interval(p, q, v, cottontail::maxfinity, cottontail::maxfinity, 0.0);

  hopper = singleton_array_hopper();
  hopper->ohr(cottontail::minfinity, &p, &q, &v);
  expect_interval(p, q, v, cottontail::minfinity, cottontail::minfinity, 0.0);
  hopper->ohr(1, &p, &q, &v);
  expect_interval(p, q, v, cottontail::minfinity, cottontail::minfinity, 0.0);
  hopper->ohr(1000, &p, &q, &v);
  expect_interval(p, q, v, 1000, 1000, 1000.0);
  hopper->ohr(2000, &p, &q, &v);
  expect_interval(p, q, v, 1000, 1000, 1000.0);
  hopper->ohr(cottontail::maxfinity, &p, &q, &v);
  expect_interval(p, q, v, cottontail::maxfinity, cottontail::maxfinity, 0.0);

  hopper = singleton_array_hopper();
  EXPECT_EQ(hopper->L(cottontail::minfinity), cottontail::minfinity);
  EXPECT_EQ(hopper->L(1), cottontail::minfinity);
  EXPECT_EQ(hopper->L(1000), 1000);
  EXPECT_EQ(hopper->L(2000), 1000);
  EXPECT_EQ(hopper->L(cottontail::maxfinity), cottontail::maxfinity);

  hopper = singleton_array_hopper();
  EXPECT_EQ(hopper->R(cottontail::minfinity), cottontail::minfinity);
  EXPECT_EQ(hopper->R(1), 1000);
  EXPECT_EQ(hopper->R(1000), 1000);
  EXPECT_EQ(hopper->R(2000), cottontail::maxfinity);
  EXPECT_EQ(hopper->R(cottontail::maxfinity), cottontail::maxfinity);
}

TEST(ArrayHopper, SingletonInterval) {
  cottontail::addr p, q;
  cottontail::fval v;

  std::unique_ptr<cottontail::Hopper> hopper =
      singleton_array_hopper(1000, 2000);
  hopper->tau(1, &p, &q, &v);
  hopper->tau(cottontail::minfinity, &p, &q, &v);
  expect_interval(p, q, v, cottontail::minfinity, cottontail::minfinity, 0.0);
  hopper->tau(1, &p, &q, &v);
  expect_interval(p, q, v, 1000, 2000, 1000.0);
  hopper->tau(1000, &p, &q, &v);
  expect_interval(p, q, v, 1000, 2000, 1000.0);
  hopper->tau(1500, &p, &q, &v);
  expect_interval(p, q, v, cottontail::maxfinity, cottontail::maxfinity, 0.0);
  hopper->tau(2000, &p, &q, &v);
  expect_interval(p, q, v, cottontail::maxfinity, cottontail::maxfinity, 0.0);
  hopper->tau(3000, &p, &q, &v);
  expect_interval(p, q, v, cottontail::maxfinity, cottontail::maxfinity, 0.0);
  hopper->tau(cottontail::maxfinity, &p, &q, &v);
  expect_interval(p, q, v, cottontail::maxfinity, cottontail::maxfinity, 0.0);

  hopper = singleton_array_hopper(1000, 2000);
  hopper->rho(1, &p, &q, &v);
  hopper->rho(cottontail::minfinity, &p, &q, &v);
  expect_interval(p, q, v, cottontail::minfinity, cottontail::minfinity, 0.0);
  hopper->rho(1, &p, &q, &v);
  expect_interval(p, q, v, 1000, 2000, 1000.0);
  hopper->rho(1000, &p, &q, &v);
  expect_interval(p, q, v, 1000, 2000, 1000.0);
  hopper->rho(1500, &p, &q, &v);
  expect_interval(p, q, v, 1000, 2000, 1000.0);
  hopper->rho(2000, &p, &q, &v);
  expect_interval(p, q, v, 1000, 2000, 1000.0);
  hopper->rho(3000, &p, &q, &v);
  expect_interval(p, q, v, cottontail::maxfinity, cottontail::maxfinity, 0.0);
  hopper->rho(cottontail::maxfinity, &p, &q, &v);
  expect_interval(p, q, v, cottontail::maxfinity, cottontail::maxfinity, 0.0);

  hopper = singleton_array_hopper(1000, 2000);
  hopper->uat(cottontail::minfinity, &p, &q, &v);
  expect_interval(p, q, v, cottontail::minfinity, cottontail::minfinity, 0.0);
  hopper->uat(1, &p, &q, &v);
  expect_interval(p, q, v, cottontail::minfinity, cottontail::minfinity, 0.0);
  hopper->uat(1000, &p, &q, &v);
  expect_interval(p, q, v, cottontail::minfinity, cottontail::minfinity, 0.0);
  hopper->uat(1500, &p, &q, &v);
  expect_interval(p, q, v, cottontail::minfinity, cottontail::minfinity, 0.0);
  hopper->uat(2000, &p, &q, &v);
  expect_interval(p, q, v, 1000, 2000, 1000.0);
  hopper->uat(3000, &p, &q, &v);
  expect_interval(p, q, v, 1000, 2000, 1000.0);
  hopper->uat(cottontail::maxfinity, &p, &q, &v);
  expect_interval(p, q, v, cottontail::maxfinity, cottontail::maxfinity, 0.0);

  hopper = singleton_array_hopper(1000, 2000);
  hopper->ohr(cottontail::minfinity, &p, &q, &v);
  expect_interval(p, q, v, cottontail::minfinity, cottontail::minfinity, 0.0);
  hopper->ohr(1, &p, &q, &v);
  expect_interval(p, q, v, cottontail::minfinity, cottontail::minfinity, 0.0);
  hopper->ohr(1000, &p, &q, &v);
  expect_interval(p, q, v, 1000, 2000, 1000.0);
  hopper->ohr(1500, &p, &q, &v);
  expect_interval(p, q, v, 1000, 2000, 1000.0);
  hopper->ohr(2000, &p, &q, &v);
  expect_interval(p, q, v, 1000, 2000, 1000.0);
  hopper->ohr(3000, &p, &q, &v);
  expect_interval(p, q, v, 1000, 2000, 1000.0);
  hopper->ohr(cottontail::maxfinity, &p, &q, &v);
  expect_interval(p, q, v, cottontail::maxfinity, cottontail::maxfinity, 0.0);

  hopper = singleton_array_hopper(1000, 2000);
  EXPECT_EQ(hopper->L(cottontail::minfinity), cottontail::minfinity);
  EXPECT_EQ(hopper->L(1), cottontail::minfinity);
  EXPECT_EQ(hopper->L(1000), cottontail::minfinity);
  EXPECT_EQ(hopper->L(1500), cottontail::minfinity);
  EXPECT_EQ(hopper->L(2000), 1000);
  EXPECT_EQ(hopper->L(3000), 1000);
  EXPECT_EQ(hopper->L(cottontail::maxfinity), cottontail::maxfinity);

  hopper = singleton_array_hopper(1000, 2000);
  EXPECT_EQ(hopper->R(cottontail::minfinity), cottontail::minfinity);
  EXPECT_EQ(hopper->R(1), 2000);
  EXPECT_EQ(hopper->R(1000), 2000);
  EXPECT_EQ(hopper->R(1500), cottontail::maxfinity);
  EXPECT_EQ(hopper->R(2000), cottontail::maxfinity);
  EXPECT_EQ(hopper->R(3000), cottontail::maxfinity);
  EXPECT_EQ(hopper->R(cottontail::maxfinity), cottontail::maxfinity);
}
