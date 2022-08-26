#ifndef COTTONTAIL_SRC_SIMPLE_IDX_H_
#define COTTONTAIL_SRC_SIMPLE_IDX_H_

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
  bool add_annotations_(const std::string &annotations_filename,
                        std::string *error) final;
  bool add_annotation_(addr feature, addr p, addr q, fval v,
                       std::string *error) final;
  bool finalize_(std::string *error) final;
  struct CacheRecord {
    addr n;
    std::shared_ptr<addr> postings;
    std::shared_ptr<addr> qostings;
    std::shared_ptr<fval> fostings;
  };
  void reset();
  CacheRecord load_cache(addr feature);
  bool add_annotations_locked(const std::string &annotations_filename,
                              std::string *error);
  void maybe_flush_additions(bool force);
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
  std::map<addr, CacheRecord> cache_;
  std::map<addr, addr> counts_;
  addr stamp_ = 0;
  std::map<addr, addr> ages_;
  addr large_threshold_ = 1024;
  addr large_limit_ = 1024 * 1024 * 1024;
  addr large_total_ = 0;
  std::mutex update_lock_;
  std::shared_ptr<bool> cannot_create_temp_files_;
  static const size_t ADD_FILE_SIZE = 256 * 1024 * 1024;
  std::shared_ptr<SimplePostingFactory> posting_factory_;
  std::vector<std::string> added_files_;
  std::unique_ptr<std::vector<Annotation>> added_;
  std::vector<std::thread> workers_;
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
