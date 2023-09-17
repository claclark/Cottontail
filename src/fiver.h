#ifndef COTTONTAIL_SRC_FIVER_H_
#define COTTONTAIL_SRC_FIVER_H_

#include <map>
#include <string>

#include "src/annotator.h"
#include "src/appender.h"
#include "src/core.h"
#include "src/featurizer.h"
#include "src/idx.h"
#include "src/simple_posting.h"
#include "src/tokenizer.h"
#include "src/txt.h"
#include "src/warren.h"
#include "src/working.h"

namespace cottontail {

class Fiver final : public Warren {
public:
  static std::shared_ptr<Fiver>
  make(std::shared_ptr<Working> working, std::shared_ptr<Featurizer> featurizer,
       std::shared_ptr<Tokenizer> tokenizer, std::string *error = nullptr,
       std::shared_ptr<Compressor> posting_compressor = nullptr,
       std::shared_ptr<Compressor> fvalue_compressor = nullptr,
       std::shared_ptr<Compressor> text_compressor = nullptr);
  static std::shared_ptr<Fiver>
  merge(const std::vector<std::shared_ptr<Fiver>> &fivers,
        std::string *error = nullptr,
        std::shared_ptr<Compressor> posting_compressor = nullptr,
        std::shared_ptr<Compressor> fvalue_compressor = nullptr,
        std::shared_ptr<Compressor> text_compressor = nullptr);
  bool pickle(const std::string &filename, std::string *error = nullptr);
  bool pickle(std::string *error = nullptr);
  bool discard(std::string *error = nullptr);
  static std::shared_ptr<Fiver>
  unpickle(const std::string &filename, std::shared_ptr<Working> working,
           std::shared_ptr<Featurizer> featurizer,
           std::shared_ptr<Tokenizer> tokenizer, std::string *error = nullptr,
           std::shared_ptr<Compressor> posting_compressor = nullptr,
           std::shared_ptr<Compressor> fvalue_compressor = nullptr,
           std::shared_ptr<Compressor> text_compressor = nullptr);
  addr relocate(addr where);
  void sequence(addr number);

  virtual ~Fiver(){};
  Fiver(const Fiver &) = delete;
  Fiver &operator=(const Fiver &) = delete;
  Fiver(Fiver &&) = delete;
  Fiver &operator=(Fiver &&) = delete;

private:
  Fiver(std::shared_ptr<Working> working,
        std::shared_ptr<Featurizer> featurizer,
        std::shared_ptr<Tokenizer> tokenizer, std::shared_ptr<Idx> idx,
        std::shared_ptr<Txt> txt)
      : Warren(working, featurizer, tokenizer, idx, txt) {
    name_ = "kitten";
  };
  std::string recipe_() final;
  bool set_parameter_(const std::string &key, const std::string &value,
                      std::string *error) final {
    safe_set(error) = "Fiver can't set its parameters";
    return false;
  };
  bool get_parameter_(const std::string &key, std::string *value,
                      std::string *error) final {
    safe_set(error) = "Fiver can't get its parameters";
    return false;
  };
  bool transaction_(std::string *error) final;
  bool ready_() final;
  void commit_() final;
  void abort_() final;
  bool built_;
  addr where_;
  addr sequence_start_, sequence_end_;
  std::shared_ptr<Compressor> posting_compressor_;
  std::shared_ptr<Compressor> fvalue_compressor_;
  std::shared_ptr<Compressor> text_compressor_;
  std::shared_ptr<std::string> text_;
  std::shared_ptr<std::vector<Annotation>> annotations_;
  std::shared_ptr<std::map<addr, std::shared_ptr<SimplePosting>>> index_;
};

} // namespace cottontail
#endif // COTTONTAIL_SRC_FIVER_H_
