#ifndef COTTONTAIL_SRC_SIMPLE_APPENDER_H_
#define COTTONTAIL_SRC_SIMPLE_APPENDER_H_

#include <fstream>
#include <memory>
#include <string>

#include "src/appender.h"
#include "src/committable.h"
#include "src/core.h"
#include "src/simple.h"
#include "src/simple_txt_io.h"
#include "src/tokenizer.h"
#include "src/working.h"

namespace cottontail {

class SimpleAppender final : public Appender {
public:
  static std::shared_ptr<Appender>
  make(const std::string &recipe, std::string *error = nullptr,
       std::shared_ptr<Working> working = nullptr,
       std::shared_ptr<Featurizer> featurizer = nullptr,
       std::shared_ptr<Tokenizer> tokenizer = nullptr,
       std::shared_ptr<Annotator> annotator = nullptr);
  static bool check(const std::string &recipe, std::string *error = nullptr);
  static bool recover(const std::string &recipe, bool commit,
                      std::string *error = nullptr,
                      std::shared_ptr<Working> working = nullptr);

  bool append(const std::string &text, addr *p, addr *q,
              std::string *error = nullptr);

  virtual ~SimpleAppender(){};
  SimpleAppender(const SimpleAppender &) = delete;
  SimpleAppender &operator=(const SimpleAppender &) = delete;
  SimpleAppender(SimpleAppender &&) = delete;
  SimpleAppender &operator=(SimpleAppender &&) = delete;

private:
  SimpleAppender(){};
  std::string recipe_() final;
  bool append_(const std::string &text, addr *p, addr *q,
               std::string *error) final;
  bool transaction_(std::string *error = nullptr) final;
  bool ready_() final;
  void commit_() final;
  void abort_() final;
  bool failed_;
  std::mutex lock_;
  std::string recipe_string_;
  std::shared_ptr<Featurizer> featurizer_;
  std::shared_ptr<Tokenizer> tokenizer_;
  std::shared_ptr<Annotator> annotator_;
  std::string txt_filename_;
  std::string raw_filename_;
  std::string map_filename_;
  std::string compressor_name_;
  std::string compressor_recipe_;
  addr text_compressor_chunk_size_;
  std::shared_ptr<SimpleTxtIO> io_;
  bool adding_;
  std::fstream txt_;
  addr address_;
  addr offset_;
};

} // namespace cottontail
#endif // COTTONTAIL_SRC_SIMPLE_APPENDER_H_
