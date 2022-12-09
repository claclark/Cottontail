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
       std::shared_ptr<Tokenizer> tokenizer,
       std::shared_ptr<Fluffle> fluffle = nullptr, std::string *error = nullptr,
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
      : Warren(working, featurizer, tokenizer, idx, txt) {
    name_ = "bigwig";
  };
  void start_() final;
  void end_() final;
  bool set_parameter_(const std::string &key, const std::string &value,
                      std::string *error) final {
    std::shared_ptr<std::map<std::string, std::string>> parameters =
        std::make_shared<std::map<std::string, std::string>>();
    fluffle_->lock.lock();
    if (fluffle_->parameters != nullptr)
      (*parameters) = *(fluffle_->parameters);
    (*parameters)[key] = value;
    fluffle_->parameters = parameters;
    fluffle_->lock.unlock();
    return true;
  };
  bool get_parameter_(const std::string &key, std::string *value,
                      std::string *error) final {
    fluffle_->lock.lock();
    std::shared_ptr<std::map<std::string, std::string>> parameters =
        fluffle_->parameters;
    if (parameters == nullptr) {
      *value = "";
    } else {
      std::map<std::string, std::string>::iterator item = parameters->find(key);
      if (item != parameters->end())
        *value = item->second;
      else
        *value = "";
    }
    fluffle_->lock.unlock();
    return true;
  };
  bool transaction_(std::string *error) final;
  bool ready_() final;
  void commit_() final;
  void abort_() final;
  std::shared_ptr<Fiver> fiver_;
  std::shared_ptr<Fluffle> fluffle_;
  std::vector<std::shared_ptr<Warren>> warrens_;
  std::shared_ptr<Compressor> posting_compressor_;
  std::shared_ptr<Compressor> fvalue_compressor_;
  std::shared_ptr<Compressor> text_compressor_;
};

} // namespace cottontail
#endif // COTTONTAIL_SRC_BIGWIG_H_
