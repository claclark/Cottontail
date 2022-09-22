#ifndef COTTONTAIL_SRC_SIMPLE_IDX_H_
#define COTTONTAIL_SRC_SIMPLE_IDX_H_

#include <condition_variable>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "src/compressor.h"
#include "src/core.h"
#include "src/hopper.h"
#include "src/idx.h"
#include "src/simple.h"
#include "src/simple_posting.h"
#include "src/working.h"

namespace cottontail {

class SimpleIdx final : public Idx {
public:
  static std::shared_ptr<Idx> make(const std::string &recipe,
                                   std::shared_ptr<Working> working,
                                   std::string *error = nullptr);
  static bool check(const std::string &recipe, std::string *error = nullptr);
  std::map<fval, addr> feature_histogram();

  virtual ~SimpleIdx(){};
  SimpleIdx(const SimpleIdx &) = delete;
  SimpleIdx &operator=(const SimpleIdx &) = delete;
  SimpleIdx(SimpleIdx &&) = delete;
  SimpleIdx &operator=(SimpleIdx &&) = delete;

private:
  SimpleIdx(){};
  std::string recipe_() final;
  std::unique_ptr<Hopper> hopper_(addr feature) final;
  addr count_(addr feature) final;
  addr vocab_() final;
  void reset_();
  std::shared_ptr<CacheRecord> load_cache(addr feature);
  bool multithreaded_ = true;
  std::string posting_compressor_name_;
  std::string posting_compressor_recipe_;
  std::shared_ptr<Compressor> posting_compressor_;
  std::string fvalue_compressor_name_;
  std::string fvalue_compressor_recipe_;
  std::shared_ptr<Compressor> fvalue_compressor_;
  std::string idx_filename_;
  std::string pst_filename_;
  std::vector<IdxRecord> pst_map_;
  std::shared_ptr<Reader> pst_;
  std::mutex cache_lock_;
  std::map<addr, std::shared_ptr<CacheRecord>> cache_;
  std::map<addr, addr> counts_;
  addr stamp_ = 0;
  std::map<addr, addr> ages_;
  addr large_threshold_ = 1024;
  addr large_limit_ = 1024 * 1024 * 1024;
  addr large_total_ = 0;
};

bool interpret_simple_idx_recipe(const std::string &recipe,
                                 std::string *fvalue_compressor_name,
                                 std::string *fvalue_compressor_recipe,
                                 std::string *posting_compressor_name,
                                 std::string *posting_compressor_recipe,
                                 std::string *error = nullptr,
                                 size_t *add_file_size = nullptr);
} // namespace cottontail
#endif // COTTONTAIL_SRC_SIMPLE_IDX_H_
