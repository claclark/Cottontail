#ifndef COTTONTAIL_SRC_BIGWIG_H_
#define COTTONTAIL_SRC_BIGWIG_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "src/fiver.h"
#include "src/fluffle.h"
#include "src/warren.h"

namespace cottontail {

class Bigwig final : public Warren {
public:
  static std::shared_ptr<Bigwig>
  make(std::shared_ptr<Working> working, std::shared_ptr<Featurizer> featurizer,
       std::shared_ptr<Tokenizer> tokenizer, std::shared_ptr<Fluffle> fluffle,
       std::string *error = nullptr,
       std::shared_ptr<std::map<std::string, std::string>> parameters = nullptr,
       std::shared_ptr<Compressor> posting_compressor = nullptr,
       std::shared_ptr<Compressor> fvalue_compressor = nullptr,
       std::shared_ptr<Compressor> text_compressor = nullptr);

  virtual ~Bigwig(){};
  Bigwig(const Bigwig &) = delete;
  Bigwig &operator=(const Bigwig &) = delete;
  Bigwig(Bigwig &&) = delete;
  Bigwig &operator=(Bigwig &&) = delete;

private:
  Bigwig(std::shared_ptr<Working> working,
         std::shared_ptr<Featurizer> featurizer,
         std::shared_ptr<Tokenizer> tokenizer, std::shared_ptr<Idx> idx,
         std::shared_ptr<Txt> txt)
      : Warren(working, featurizer, tokenizer, idx, txt){};
  void start_() final;
  void end_() final;
  bool set_parameter_(const std::string &key, const std::string &value,
                      std::string *error) final {
    safe_set(error) = "Bigwig can't set its parameters";
    return false;
  };
  bool get_parameter_(const std::string &key, std::string *value,
                      std::string *error) final {
    if (parameters_ == nullptr) {
      *value = "";
      return true;
    }
    std::map<std::string, std::string>::iterator item = parameters_->find(key);
    if (item != parameters_->end())
      *value = item->second;
    else
      *value = "";
    return true;
  };
  bool transaction_(std::string *error) final;
  bool ready_() final;
  void commit_() final;
  void abort_() final;
  std::shared_ptr<std::map<std::string, std::string>> parameters_;
  std::shared_ptr<Compressor> posting_compressor_;
  std::shared_ptr<Compressor> fvalue_compressor_;
  std::shared_ptr<Compressor> text_compressor_;
  std::shared_ptr<Fiver> fiver_;
  std::shared_ptr<Fluffle> fluffle_;
  std::vector<std::shared_ptr<Warren>> warrens_;
};

} // namespace cottontail
#endif // COTTONTAIL_SRC_BIGWIG_H_
