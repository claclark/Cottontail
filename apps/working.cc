#include <algorithm>
#include <cstring>
#include <iostream>
#include <map>
#include <mutex>
#include <regex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#define ASSERT_EQ(a, b) (assert((a) == (b)))
#define ASSERT_NE(a, b) (assert((a) != (b)))
#define ASSERT_TRUE(a) (assert((a)))
#define ASSERT_FALSE(a) (assert(!(a)))
#define EXPECT_EQ(a, b) (assert((a) == (b)))
#define EXPECT_NE(a, b) (assert((a) != (b)))
#define EXPECT_TRUE(a) (assert((a)))
#define EXPECT_FALSE(a) (assert(!(a)))

#include "src/array_hopper.h"
#include "src/bits.h"
#include "src/compressor.h"
#include "src/core.h"
#include "src/featurizer.h"
#include "src/hopper.h"
#include "src/idx.h"
#include "src/ranking.h"
#include "src/simple_builder.h"
#include "src/simple_idx.h"
#include "src/simple_posting.h"
#include "src/simple_txt.h"
#include "src/tokenizer.h"
#include "src/txt.h"
#include "src/warren.h"
#include "src/working.h"

void usage(std::string program_name) {
  std::cerr << "usage: " << program_name << " [--burrow burrow] \n";
}

int main(int argc, char **argv) {
  cottontail::addr n = 763;
  std::string burrow = cottontail::DEFAULT_BURROW;
  std::string error;
  cottontail::addr p, q, v;
  {
    std::shared_ptr<cottontail::Working> working =
        cottontail::Working::mkdir(burrow);
    ASSERT_NE(working, nullptr);
    std::string options = "";
    std::shared_ptr<cottontail::Builder> builder =
        cottontail::SimpleBuilder::make(working, options, &error);
    ASSERT_NE(builder, nullptr);
    std::string contents;
    for (cottontail::addr i = 0; i < n; i++) {
      contents += std::to_string(i);
      contents += " ";
    }
    ASSERT_TRUE(builder->add_text(contents, &p, &q));
    for (cottontail::addr i = 0; i < n; i++)
      ASSERT_TRUE(builder->add_annotation("sub1", i, i, i - 1));
    for (cottontail::addr i = 0; i < n; i++)
      ASSERT_TRUE(builder->add_annotation("mult3", i, i, 3 * i));
    for (cottontail::addr i = 0; i < n; i++)
      if (i % 3 == 0)
        ASSERT_TRUE(builder->add_annotation("foo", i, i, (cottontail::addr)99));
      else if (i % 3 == 1)
        ASSERT_TRUE(
            builder->add_annotation("foo", i, i, (cottontail::addr)77777));
      else
        ASSERT_TRUE(builder->add_annotation("foo", i, i, (cottontail::addr)5));
    ASSERT_TRUE(builder->finalize());
  }
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::Warren::make("simple", burrow, &error);
  warren->start();
  cottontail::addr i;
  std::unique_ptr<cottontail::Hopper> h;
  h = warren->hopper_from_gcl("(@ sub1)", &error);
  h->ohr(5, &p, &q, &v);
  ASSERT_EQ(5, p);
  ASSERT_EQ(p, q);
  ASSERT_EQ(v, 0);
  i = -1;
  for (h->tau(cottontail::minfinity + 1, &p, &q, &v); p < cottontail::maxfinity;
       h->tau(p + 1, &p, &q, &v)) {
    ASSERT_EQ(i, p);
    ASSERT_EQ(p, q);
    ASSERT_EQ(v, 0);
    i += 1;
  }
  ASSERT_EQ(i, n - 1);
  h->uat(cottontail::maxfinity - 1, &p, &q, &v);
  ASSERT_EQ(n - 2, p);
  ASSERT_EQ(p, q);
  ASSERT_EQ(v, 0);
  h->rho(cottontail::minfinity + 1, &p, &q, &v);
  ASSERT_EQ(-1, p);
  ASSERT_EQ(p, q);
  ASSERT_EQ(v, 0);
  h = warren->hopper_from_gcl("(@ mult3)", &error);
  h->rho(7, &p, &q, &v);
  ASSERT_EQ(9, p);
  ASSERT_EQ(p, q);
  ASSERT_EQ(v, 0);
  i = 0;
  for (h->tau(cottontail::minfinity + 1, &p, &q, &v); p < cottontail::maxfinity;
       h->tau(p + 1, &p, &q, &v)) {
    ASSERT_EQ(i, p);
    ASSERT_EQ(p, q);
    ASSERT_EQ(v, 0);
    i += 3;
  }
  ASSERT_EQ(i, 3 * n);
  h->ohr(5, &p, &q, &v);
  ASSERT_EQ(3, p);
  ASSERT_EQ(p, q);
  ASSERT_EQ(v, 0);
  h->uat(14, &p, &q, &v);
  ASSERT_EQ(12, p);
  ASSERT_EQ(p, q);
  ASSERT_EQ(v, 0);
  h = warren->hopper_from_gcl("(@ foo)", &error);
  h->tau(7, &p, &q, &v);
  ASSERT_EQ(99, p);
  ASSERT_EQ(p, q);
  ASSERT_EQ(v, 0);
  h = warren->hopper_from_gcl("(@ foo)", &error);
  h->uat(7, &p, &q, &v);
  ASSERT_EQ(5, p);
  ASSERT_EQ(p, q);
  ASSERT_EQ(v, 0);
  h->ohr(0, &p, &q, &v);
  ASSERT_EQ(cottontail::minfinity, p);
  ASSERT_EQ(p, q);
  ASSERT_EQ(v, 0);
  h->rho(0, &p, &q, &v);
  ASSERT_EQ(5, p);
  ASSERT_EQ(p, q);
  ASSERT_EQ(v, 0);
  h->rho(100, &p, &q, &v);
  ASSERT_EQ(77777, p);
  ASSERT_EQ(p, q);
  ASSERT_EQ(v, 0);
  warren->end();
}
