#include "gtest/gtest.h"

#include "src/core.h"
#include "src/hopper.h"

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
