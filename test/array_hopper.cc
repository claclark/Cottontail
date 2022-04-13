#include <memory>
#include <string>

#include "gtest/gtest.h"

#include "src/array_hopper.h"
#include "src/core.h"

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
  cottontail::ArrayHopper hopper(n, postings, qostings, fostings);
  cottontail::addr p, q;
  cottontail::fval v;

  hopper.ohr(cottontail::minfinity, &p, &q, &v);
  EXPECT_EQ(p, cottontail::minfinity);
  EXPECT_EQ(q, cottontail::minfinity);
  EXPECT_EQ(v, 0.0);
  hopper.ohr(-99, &p, &q, &v);
  EXPECT_EQ(p, cottontail::minfinity);
  EXPECT_EQ(q, cottontail::minfinity);
  EXPECT_EQ(v, 0.0);
  hopper.ohr(1, &p, &q, &v);
  EXPECT_EQ(p, 1);
  EXPECT_EQ(q, 11);
  EXPECT_EQ(v, 1.0);
  hopper.ohr(12, &p, &q, &v);
  EXPECT_EQ(p, 10);
  EXPECT_EQ(q, 15);
  EXPECT_EQ(v, 2.0);
  hopper.ohr(20, &p, &q, &v);
  EXPECT_EQ(p, 20);
  EXPECT_EQ(q, 20);
  EXPECT_EQ(v, 3.0);
  hopper.ohr(99, &p, &q, &v);
  EXPECT_EQ(p, 20);
  EXPECT_EQ(q, 20);
  EXPECT_EQ(v, 3.0);
  hopper.ohr(1000, &p, &q, &v);
  EXPECT_EQ(p, 100);
  EXPECT_EQ(q, 110);
  EXPECT_EQ(v, 4.0);
  hopper.ohr(cottontail::maxfinity, &p, &q, &v);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  EXPECT_EQ(v, 0.0);

  hopper.uat(cottontail::minfinity, &p, &q, &v);
  EXPECT_EQ(p, cottontail::minfinity);
  EXPECT_EQ(q, cottontail::minfinity);
  EXPECT_EQ(v, 0.0);
  hopper.uat(-99, &p, &q, &v);
  EXPECT_EQ(p, cottontail::minfinity);
  EXPECT_EQ(q, cottontail::minfinity);
  EXPECT_EQ(v, 0.0);
  hopper.uat(1, &p, &q, &v);
  EXPECT_EQ(p, cottontail::minfinity);
  EXPECT_EQ(q, cottontail::minfinity);
  EXPECT_EQ(v, 0.0);
  hopper.uat(12, &p, &q, &v);
  EXPECT_EQ(p, 1);
  EXPECT_EQ(q, 11);
  EXPECT_EQ(v, 1.0);
  hopper.uat(16, &p, &q, &v);
  EXPECT_EQ(p, 10);
  EXPECT_EQ(q, 15);
  EXPECT_EQ(v, 2.0);
  hopper.uat(20, &p, &q, &v);
  EXPECT_EQ(p, 20);
  EXPECT_EQ(q, 20);
  EXPECT_EQ(v, 3.0);
  hopper.uat(1000, &p, &q, &v);
  EXPECT_EQ(p, 100);
  EXPECT_EQ(q, 110);
  EXPECT_EQ(v, 4.0);
  hopper.uat(cottontail::maxfinity, &p, &q, &v);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  EXPECT_EQ(v, 0.0);

  hopper.rho(cottontail::minfinity, &p, &q, &v);
  EXPECT_EQ(p, cottontail::minfinity);
  EXPECT_EQ(q, cottontail::minfinity);
  EXPECT_EQ(v, 0.0);
  hopper.rho(-99, &p, &q, &v);
  EXPECT_EQ(p, 1);
  EXPECT_EQ(q, 11);
  EXPECT_EQ(v, 1.0);
  hopper.rho(1, &p, &q, &v);
  EXPECT_EQ(p, 1);
  EXPECT_EQ(q, 11);
  EXPECT_EQ(v, 1.0);
  hopper.rho(2, &p, &q, &v);
  EXPECT_EQ(p, 1);
  EXPECT_EQ(q, 11);
  EXPECT_EQ(v, 1.0);
  hopper.rho(12, &p, &q, &v);
  EXPECT_EQ(p, 10);
  EXPECT_EQ(q, 15);
  EXPECT_EQ(v, 2.0);
  hopper.rho(100, &p, &q, &v);
  EXPECT_EQ(p, 100);
  EXPECT_EQ(q, 110);
  EXPECT_EQ(v, 4.0);
  hopper.rho(1000, &p, &q, &v);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  EXPECT_EQ(v, 0.0);
  hopper.rho(cottontail::maxfinity, &p, &q, &v);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  EXPECT_EQ(v, 0.0);

  hopper.tau(cottontail::minfinity, &p, &q, &v);
  EXPECT_EQ(p, cottontail::minfinity);
  EXPECT_EQ(q, cottontail::minfinity);
  EXPECT_EQ(v, 0.0);
  hopper.tau(-99, &p, &q, &v);
  EXPECT_EQ(p, 1);
  EXPECT_EQ(q, 11);
  EXPECT_EQ(v, 1.0);
  hopper.tau(1, &p, &q, &v);
  EXPECT_EQ(p, 1);
  EXPECT_EQ(q, 11);
  EXPECT_EQ(v, 1.0);
  hopper.tau(2, &p, &q, &v);
  EXPECT_EQ(p, 10);
  EXPECT_EQ(q, 15);
  EXPECT_EQ(v, 2.0);
  hopper.tau(21, &p, &q, &v);
  EXPECT_EQ(p, 100);
  EXPECT_EQ(q, 110);
  EXPECT_EQ(v, 4.0);
  hopper.tau(100, &p, &q, &v);
  EXPECT_EQ(p, 100);
  EXPECT_EQ(q, 110);
  EXPECT_EQ(v, 4.0);
  hopper.tau(1000, &p, &q, &v);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  EXPECT_EQ(v, 0.0);
  hopper.tau(cottontail::maxfinity, &p, &q, &v);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  EXPECT_EQ(v, 0.0);
}
