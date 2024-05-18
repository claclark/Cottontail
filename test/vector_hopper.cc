#include "src/vector_hopper.h"

#include <memory>
#include <vector>

#include "gtest/gtest.h"

#include "src/cottontail.h" 

TEST(VectorHopper, Basic) {
  std::vector<std::unique_ptr<cottontail::Hopper>> hoppers;
  hoppers.push_back(std::make_unique<cottontail::SingletonHopper>(3, 40, -3.0));
  hoppers.push_back(std::make_unique<cottontail::SingletonHopper>(2, 12, 2.0));
  hoppers.push_back(
      std::make_unique<cottontail::SingletonHopper>(20, 30, 20.0));
  hoppers.push_back(std::make_unique<cottontail::SingletonHopper>(0, 10, 0.0));
  hoppers.push_back(
      std::make_unique<cottontail::SingletonHopper>(10, 20, 10.0));
  hoppers.push_back(std::make_unique<cottontail::SingletonHopper>(4, 14, -4.0));
  hoppers.push_back(std::make_unique<cottontail::SingletonHopper>(3, 13, 3.0));
  hoppers.push_back(std::make_unique<cottontail::SingletonHopper>(4, 14, 4.0));
  hoppers.push_back(std::make_unique<cottontail::SingletonHopper>(2, 20, -2.0));
  hoppers.push_back(std::make_unique<cottontail::SingletonHopper>(1, 11, 1.0));
  std::unique_ptr<cottontail::Hopper> hopper =
      cottontail::gcl::VectorHopper::make(&hoppers, false);
  ASSERT_NE(hopper, nullptr);
  EXPECT_EQ(hoppers.size(), 0);
  cottontail::addr p, q;
  cottontail::fval v;
  size_t i;
  i = 0;
  for (hopper->tau(cottontail::minfinity + 1, &p, &q, &v);
       p < cottontail::maxfinity; hopper->tau(p + 1, &p, &q, &v)) {
    EXPECT_EQ(p + 10, q);
    EXPECT_EQ((cottontail::fval)p, v);
    i++;
  }
  EXPECT_EQ(i, 7);
  i = 0;
  for (hopper->rho(cottontail::minfinity + 1, &p, &q, &v);
       q < cottontail::maxfinity; hopper->rho(q + 1, &p, &q, &v)) {
    EXPECT_EQ(p + 10, q);
    EXPECT_EQ((cottontail::fval)p, v);
    i++;
  }
  EXPECT_EQ(i, 7);
  i = 0;
  for (hopper->uat(cottontail::maxfinity - 1, &p, &q, &v);
       q > cottontail::minfinity; hopper->uat(q - 1, &p, &q, &v)) {
    EXPECT_EQ(p + 10, q);
    EXPECT_EQ((cottontail::fval)p, v);
    i++;
  }
  EXPECT_EQ(i, 7);
  i = 0;
  for (hopper->ohr(cottontail::maxfinity - 1, &p, &q, &v);
       p > cottontail::minfinity; hopper->ohr(p - 1, &p, &q, &v)) {
    EXPECT_EQ(p + 10, q);
    EXPECT_EQ((cottontail::fval)p, v);
    i++;
  }
  EXPECT_EQ(i, 7);
  hopper->tau(cottontail::maxfinity, &p, &q);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  hopper->tau(15, &p, &q, &v);
  EXPECT_EQ(p, 20);
  EXPECT_EQ(q, 30);
  EXPECT_EQ(v, 20.0);
  hopper->tau(5, &p, &q, &v);
  EXPECT_EQ(p, 10);
  EXPECT_EQ(q, 20);
  EXPECT_EQ(v, 10.0);
  hopper->tau(cottontail::minfinity, &p, &q);
  EXPECT_EQ(p, cottontail::minfinity);
  EXPECT_EQ(q, cottontail::minfinity);
  hopper->rho(cottontail::maxfinity, &p, &q);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  hopper->rho(21, &p, &q, &v);
  EXPECT_EQ(p, 20);
  EXPECT_EQ(q, 30);
  EXPECT_EQ(v, 20.0);
  hopper->rho(15, &p, &q, &v);
  EXPECT_EQ(p, 10);
  EXPECT_EQ(q, 20);
  EXPECT_EQ(v, 10.0);
  hopper->rho(cottontail::minfinity, &p, &q);
  EXPECT_EQ(p, cottontail::minfinity);
  EXPECT_EQ(q, cottontail::minfinity);
  hopper->uat(cottontail::minfinity, &p, &q);
  EXPECT_EQ(p, cottontail::minfinity);
  EXPECT_EQ(q, cottontail::minfinity);
  hopper->uat(15, &p, &q, &v);
  EXPECT_EQ(p, 4);
  EXPECT_EQ(q, 14);
  EXPECT_EQ(v, 4.0);
  hopper->uat(25, &p, &q, &v);
  EXPECT_EQ(p, 10);
  EXPECT_EQ(q, 20);
  EXPECT_EQ(v, 10.0);
  hopper->uat(cottontail::maxfinity, &p, &q);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  hopper->ohr(cottontail::minfinity, &p, &q);
  EXPECT_EQ(p, cottontail::minfinity);
  EXPECT_EQ(q, cottontail::minfinity);
  hopper->ohr(9, &p, &q, &v);
  EXPECT_EQ(p, 4);
  EXPECT_EQ(q, 14);
  EXPECT_EQ(v, 4.0);
  hopper->ohr(15, &p, &q, &v);
  EXPECT_EQ(p, 10);
  EXPECT_EQ(q, 20);
  EXPECT_EQ(v, 10.0);
  hopper->ohr(cottontail::maxfinity, &p, &q);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
}

TEST(VectorHopper, Special) {
  cottontail::fval v;
  cottontail::addr p, q;
  std::unique_ptr<cottontail::Hopper> hopper;
  hopper = cottontail::gcl::VectorHopper::make(nullptr, false);
  ASSERT_NE(hopper, nullptr);
  hopper->ohr(cottontail::minfinity, &p, &q);
  EXPECT_EQ(p, cottontail::minfinity);
  EXPECT_EQ(q, cottontail::minfinity);
  hopper->uat(cottontail::maxfinity, &p, &q);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  std::vector<std::unique_ptr<cottontail::Hopper>> hoppers;
  hopper = cottontail::gcl::VectorHopper::make(&hoppers, false);
  ASSERT_NE(hopper, nullptr);
  EXPECT_EQ(hoppers.size(), 0);
  hopper->rho(cottontail::minfinity, &p, &q);
  EXPECT_EQ(p, cottontail::minfinity);
  EXPECT_EQ(q, cottontail::minfinity);
  hopper->tau(cottontail::maxfinity, &p, &q);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  hoppers.push_back(
      std::make_unique<cottontail::SingletonHopper>(100, 200, 1.0));
  hopper = cottontail::gcl::VectorHopper::make(&hoppers, false);
  ASSERT_NE(hopper, nullptr);
  EXPECT_EQ(hoppers.size(), 0);
  hopper->tau(cottontail::minfinity, &p, &q);
  EXPECT_EQ(p, cottontail::minfinity);
  EXPECT_EQ(q, cottontail::minfinity);
  hopper->tau(0, &p, &q, &v);
  EXPECT_EQ(p, 100);
  EXPECT_EQ(q, 200);
  EXPECT_EQ(v, 1.0);
  hopper->tau(101, &p, &q, &v);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  EXPECT_EQ(v, 0.0);
  hopper->tau(cottontail::maxfinity, &p, &q);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
}
