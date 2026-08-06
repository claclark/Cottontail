#include <memory>
#include <string>

#include "gtest/gtest.h"

#include "gcl/optimizer.h"
#include "gcl/parse.h"
#include "src/ascii_tokenizer.h"
#include "src/core.h"
#include "src/featurizer.h"
#include "src/hopper.h"
#include "src/idx.h"
#include "src/null_txt.h"
#include "src/warren.h"

namespace {

class NumericFeaturizer final : public cottontail::Featurizer {
public:
  NumericFeaturizer(){};
  virtual ~NumericFeaturizer(){};
  NumericFeaturizer(const NumericFeaturizer &) = delete;
  NumericFeaturizer &operator=(const NumericFeaturizer &) = delete;
  NumericFeaturizer(NumericFeaturizer &&) = delete;
  NumericFeaturizer &operator=(NumericFeaturizer &&) = delete;

private:
  std::string name_() final { return "numeric"; }
  std::string recipe_() final { return ""; }
  cottontail::addr featurize_(const char *key, cottontail::addr length) final {
    if (length <= 0)
      return 0;
    cottontail::addr feature = 0;
    for (cottontail::addr i = 0; i < length; i++) {
      if (key[i] < '0' || key[i] > '9')
        return 0;
      feature = 10 * feature + key[i] - '0';
    }
    return feature;
  }
  std::string translate_(cottontail::addr feature) final {
    return std::to_string(feature);
  }
};

class CountingIdx final : public cottontail::Idx {
public:
  CountingIdx(){};
  virtual ~CountingIdx(){};
  CountingIdx(const CountingIdx &) = delete;
  CountingIdx &operator=(const CountingIdx &) = delete;
  CountingIdx(CountingIdx &&) = delete;
  CountingIdx &operator=(CountingIdx &&) = delete;

private:
  std::string recipe_() final { return ""; }
  std::unique_ptr<cottontail::Hopper> hopper_(cottontail::addr feature) final {
    return std::make_unique<cottontail::EmptyHopper>();
  }
  cottontail::addr count_(cottontail::addr feature) final { return feature; }
  cottontail::addr vocab_() final { return 0; }
};

class OptimizerWarren final : public cottontail::Warren {
public:
  OptimizerWarren()
      : Warren(nullptr, std::make_shared<NumericFeaturizer>(),
               cottontail::AsciiTokenizer::make(false),
               std::make_shared<CountingIdx>(), cottontail::NullTxt::make("")) {
  }
  virtual ~OptimizerWarren(){};
  OptimizerWarren(const OptimizerWarren &) = delete;
  OptimizerWarren &operator=(const OptimizerWarren &) = delete;
  OptimizerWarren(OptimizerWarren &&) = delete;
  OptimizerWarren &operator=(OptimizerWarren &&) = delete;

private:
  bool set_parameter_(const std::string &key, const std::string &value,
                      std::string *error) final {
    (void)key;
    (void)value;
    (void)error;
    return true;
  }
  bool get_parameter_(const std::string &key, std::string *value,
                      std::string *error) final {
    (void)key;
    (void)error;
    if (value != nullptr)
      *value = "";
    return true;
  }
};

std::string optimize(cottontail::Warren *warren, const std::string &query) {
  std::string error;
  std::shared_ptr<cottontail::gcl::SExpression> expr =
      cottontail::gcl::SExpression::from_string(query, &error);
  if (expr == nullptr) {
    ADD_FAILURE() << error;
    return "";
  }
  expr = expr->expand_phrases(warren->tokenizer());
  expr = cottontail::gcl::Optimizer::optimize(expr, warren);
  return expr->to_string();
}

class ScopedOptimization final {
public:
  ScopedOptimization() { cottontail::gcl::Optimizer::enable(); }
  ~ScopedOptimization() { cottontail::gcl::Optimizer::disable(); }
  ScopedOptimization(const ScopedOptimization &) = delete;
  ScopedOptimization &operator=(const ScopedOptimization &) = delete;
};

} // namespace

TEST(OptimizerTest, EstimatesMemoryFromString) {
  OptimizerWarren warren;
  warren.start();
  cottontail::addr posting_bytes = 3 * sizeof(cottontail::addr);
  EXPECT_EQ(cottontail::gcl::Optimizer::estimate_memory(
                "(+ 10 20 10 (# 3))", &warren),
            30 * posting_bytes);
  EXPECT_EQ(cottontail::gcl::Optimizer::estimate_memory(
                "(+ \"10 20\" 10)", &warren),
            30 * posting_bytes);
  EXPECT_EQ(cottontail::gcl::Optimizer::estimate_memory("(+)", &warren), 0);
  EXPECT_EQ(cottontail::gcl::Optimizer::estimate_memory("10", nullptr), 0);
  warren.end();
}

TEST(OptimizerTest, EstimatesMemoryFromExpression) {
  OptimizerWarren warren;
  warren.start();
  std::string error;
  std::shared_ptr<cottontail::gcl::SExpression> expr =
      cottontail::gcl::SExpression::from_string("(+ \"10 20\" 30)", &error);
  ASSERT_NE(expr, nullptr) << error;
  cottontail::addr posting_bytes = 3 * sizeof(cottontail::addr);
  EXPECT_EQ(cottontail::gcl::Optimizer::estimate_memory(expr, &warren),
            60 * posting_bytes);
  EXPECT_EQ(cottontail::gcl::Optimizer::estimate_memory(
                std::shared_ptr<cottontail::gcl::SExpression>(), &warren),
            0);
  warren.end();
}

TEST(OptimizerTest, TopLevelContainmentRemainsUnchanged) {
  ScopedOptimization optimization;
  OptimizerWarren warren;
  warren.start();
  EXPECT_EQ(optimize(&warren, "(<< (^ 30 10 20) 5)"),
            "(<< (^ 30 10 20) 5)");
  EXPECT_EQ(optimize(&warren, "(>> (^ 30 10 20) 5)"),
            "(>> (^ 30 10 20) 5)");
  warren.end();
}

TEST(OptimizerTest, OneOfMaterializesContainmentChildren) {
  ScopedOptimization optimization;
  OptimizerWarren warren;
  warren.start();
  EXPECT_EQ(optimize(&warren, "(+ (<< (^ 30 10) 5) 40)"),
            "(+ (materialize (<< (^ 30 10) 5)) 40)");
  EXPECT_EQ(optimize(&warren, "(+ (^ 1 (<< 2 3) 4) 5)"),
            "(+ (^ 1 (materialize (<< 2 3)) 4) 5)");
  EXPECT_EQ(optimize(&warren, "(+ \"10 20\" 30)"),
            "(+ (materialize (>> (# 2) (... 10 20))) 30)");
  warren.end();
}

TEST(OptimizerTest, MaterializedContainmentIsAtomic) {
  ScopedOptimization optimization;
  OptimizerWarren warren;
  warren.start();
  EXPECT_EQ(
      optimize(&warren, "(+ (materialize (<< (^ 1 (<< 2 3)) 4)) 5)"),
      "(+ (materialize (<< (^ 1 (<< 2 3)) 4)) 5)");
  warren.end();
}

TEST(OptimizerTest, NestedOneOfStartsANewMaterializationContext) {
  ScopedOptimization optimization;
  OptimizerWarren warren;
  warren.start();
  EXPECT_EQ(
      optimize(&warren, "(+ (<< (+ (<< 1 2) 3) 4) 5)"),
      "(+ (materialize (<< (+ (materialize (<< 1 2)) 3) 4)) 5)");
  warren.end();
}

TEST(OptimizerTest, NonMatchingShapesRemainUnchanged) {
  ScopedOptimization optimization;
  OptimizerWarren warren;
  warren.start();
  EXPECT_EQ(optimize(&warren, "(^ 30 10 20)"), "(^ 30 10 20)");
  EXPECT_EQ(optimize(&warren, "(<< (... 30 10 20) 5)"),
            "(<< (... 30 10 20) 5)");
  EXPECT_EQ(optimize(&warren, "\"30 10 20\""), "(>> (# 3) (... 30 10 20))");
  warren.end();
}
