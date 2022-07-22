#ifndef COTTONTAIL_SRC_SIMPLE_BUILDER_H_
#define COTTONTAIL_SRC_SIMPLE_BUILDER_H_

#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "src/builder.h"
#include "src/core.h"
#include "src/featurizer.h"
#include "src/simple.h"
#include "src/simple_posting.h"
#include "src/simple_txt_io.h"
#include "src/tokenizer.h"
#include "src/working.h"

namespace cottontail {

class SimpleBuilder final : public Builder {
public:
  static std::shared_ptr<Builder> make(std::shared_ptr<Working> working,
                                       const std::string &recipe,
                                       std::string *error = nullptr);
  static bool check(const std::string &recipe, std::string *error = nullptr);
  static std::shared_ptr<Builder>
  make(std::shared_ptr<Working> working, std::shared_ptr<Featurizer> featurizer,
       std::shared_ptr<Tokenizer> tokenizer, std::string *error = nullptr,
       size_t tok_file_size = DEFAULT_TOK_FILE_SIZE,
       size_t ann_file_size = DEFAULT_ANN_FILE_SIZE,
       std::string posting_compressor_name = POSTING_COMPRESSOR_NAME,
       std::string posting_compressor_recipe = POSTING_COMPRESSOR_RECIPE,
       std::string fvalue_compressor_name = FVALUE_COMPRESSOR_NAME,
       std::string fvalue_compressor_recipe = FVALUE_COMPRESSOR_RECIPE,
       std::string text_compressor_name = TEXT_COMPRESSOR_NAME,
       std::string text_compressor_recipe = TEXT_COMPRESSOR_RECIPE);

  virtual ~SimpleBuilder(){};
  SimpleBuilder(const SimpleBuilder &) = delete;
  SimpleBuilder &operator=(const SimpleBuilder &) = delete;
  SimpleBuilder(SimpleBuilder &&) = delete;
  SimpleBuilder &operator=(SimpleBuilder &&) = delete;

private:
  SimpleBuilder(){};
  bool add_text_(const std::string &text, addr *p, addr *q,
                 std::string *error) final;
  bool add_annotation_(const std::string &tag, addr p, addr q, fval v,
                       std::string *error) final;
  bool finalize_(std::string *error) final;
  bool maybe_flush_tokens(bool force, std::string *error);
  bool maybe_flush_annotations(bool force, std::string *error);
  bool build_index(std::string *error);

  // Default changable mostly for testing
  static const size_t DEFAULT_TOK_FILE_SIZE = 256 * 1024 * 1024;
  static const size_t DEFAULT_ANN_FILE_SIZE = 256 * 1024 * 1024;
  std::shared_ptr<Working> working_;
  std::shared_ptr<Featurizer> featurizer_;
  std::shared_ptr<Tokenizer> tokenizer_;
  std::string posting_compressor_name_;
  std::string posting_compressor_recipe_;
  std::string fvalue_compressor_name_;
  std::string fvalue_compressor_recipe_;
  std::string text_compressor_name_;
  std::string text_compressor_recipe_;
  std::shared_ptr<SimpleTxtIO> io_;
  addr address_;
  addr offset_;
  std::string txt_filename_;
  std::fstream txt_;
  std::string pst_filename_;
  std::string idx_filename_;
  std::queue<std::string> tokq_;
  std::unique_ptr<std::vector<TokRecord>> tokens_;
  size_t tok_file_size_;
  std::queue<std::string> annq_;
  std::unique_ptr<std::vector<Annotation>> annotations_;
  size_t ann_file_size_;
  std::shared_ptr<SimplePostingFactory> posting_factory_;
  bool multithreaded_ = true;
  std::vector<std::thread> workers_;
};

bool check_annotations(const std::string &filename, std::string *error);
bool sort_annotations(std::shared_ptr<Working> working,
                      const std::string &in_filename, std::string *out_filename,
                      std::string *error);
} // namespace cottontail

#endif // COTTONTAIL_SRC_SIMPLE_BUILDER_H_
