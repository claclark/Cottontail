#ifndef COTTONTAIL_SRC_SIMPLE_ANNOTATOR_H_
#define COTTONTAIL_SRC_SIMPLE_ANNOTATOR_H_

#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "src/annotator.h"
#include "src/compressor.h"
#include "src/core.h"
#include "src/simple.h"
#include "src/simple_posting.h"
#include "src/working.h"

namespace cottontail {

class SimpleAnnotator : public Annotator {
public:
  static std::shared_ptr<Annotator> make(const std::string &recipe,
                                         std::shared_ptr<Working> working,
                                         std::string *error = nullptr);
  static bool check(const std::string &recipe, std::string *error = nullptr);
  static bool recover(const std::string &recipe, bool commit,
                      std::shared_ptr<Working> working,
                      std::string *error = nullptr);

  virtual ~SimpleAnnotator(){};
  SimpleAnnotator(const SimpleAnnotator &) = delete;
  SimpleAnnotator &operator=(const SimpleAnnotator &) = delete;
  SimpleAnnotator(SimpleAnnotator &&) = delete;
  SimpleAnnotator &operator=(SimpleAnnotator &&) = delete;

private:
  SimpleAnnotator(){};
  std::string recipe_() final;
  bool transaction_(std::string *error) final;
  bool annotate_(addr feature, addr p, addr q, fval v,
                 std::string *error) final;
  bool ready_() final;
  void commit_() final;
  void abort_() final;
  void maybe_flush_additions(bool force);
  void wait_for_workers();
  void cleanup_temporary_files();
  std::string the_recipe_;
  bool failed_;
  bool adding_;
  bool isready_;
  bool multithreaded_;
  std::shared_ptr<Working> working_;
  std::string posting_compressor_name_;
  std::string posting_compressor_recipe_;
  std::shared_ptr<Compressor> posting_compressor_;
  std::string fvalue_compressor_name_;
  std::string fvalue_compressor_recipe_;
  std::shared_ptr<Compressor> fvalue_compressor_;
  std::string idx_filename_;
  std::string pst_filename_;
  std::mutex lock_;
  std::shared_ptr<bool> cannot_create_temp_files_;
  static const size_t ADD_FILE_SIZE = 256 * 1024 * 1024;
  size_t add_file_size_ = ADD_FILE_SIZE;
  std::shared_ptr<SimplePostingFactory> posting_factory_;
  std::vector<std::string> added_files_;
  std::unique_ptr<std::vector<Annotation>> added_;
  std::vector<std::thread> workers_;
};
} // namespace cottontail
#endif // COTTONTAIL_SRC_SIMPLE_ANNOTATOR_H_
