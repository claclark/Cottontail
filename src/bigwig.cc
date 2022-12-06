#include "src/bigwig.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "src/annotator.h"
#include "src/appender.h"
#include "src/compressor.h"
#include "src/core.h"
#include "src/featurizer.h"
#include "src/fluffle.h"
#include "src/hopper.h"
#include "src/warren.h"

namespace cottontail {

class BigwigAnnotator final : public Annotator {
public:
  static std::shared_ptr<Annotator> make(std::shared_ptr<Fiver> fiver,
                                         std::string *error = nullptr) {
    if (fiver == nullptr) {
      safe_set(error) = "BigwigAnnotator got null Fiver";
      return nullptr;
    }
    std::shared_ptr<BigwigAnnotator> annotator =
        std::shared_ptr<BigwigAnnotator>(new BigwigAnnotator());
    annotator->fiver_ = fiver;
    return annotator;
  }

  virtual ~BigwigAnnotator(){};
  BigwigAnnotator(const BigwigAnnotator &) = delete;
  BigwigAnnotator &operator=(const BigwigAnnotator &) = delete;
  BigwigAnnotator(BigwigAnnotator &&) = delete;
  BigwigAnnotator &operator=(BigwigAnnotator &&) = delete;

private:
  BigwigAnnotator(){};
  std::string recipe_() final { return ""; };
  bool annotate_(addr feature, addr p, addr q, fval v,
                 std::string *error) final {
    return fiver_->annotator()->annotate(feature, p, q, v, error);
  };
  std::shared_ptr<Fiver> fiver_;
};

class BigwigAppender final : public Appender {
public:
  static std::shared_ptr<Appender> make(std::shared_ptr<Fiver> fiver,
                                        std::string *error = nullptr) {
    if (fiver == nullptr) {
      safe_set(error) = "BigwigAppender got null Fiver";
      return nullptr;
    }
    std::shared_ptr<BigwigAppender> annotator =
        std::shared_ptr<BigwigAppender>(new BigwigAppender());
    annotator->fiver_ = fiver;
    return annotator;
  }

  virtual ~BigwigAppender(){};
  BigwigAppender(const BigwigAppender &) = delete;
  BigwigAppender &operator=(const BigwigAppender &) = delete;
  BigwigAppender(BigwigAppender &&) = delete;
  BigwigAppender &operator=(BigwigAppender &&) = delete;

private:
  BigwigAppender(){};
  std::string recipe_() final { return ""; };
  bool append_(const std::string &text, addr *p, addr *q, std::string *error) {
    return fiver_->appender()->append(text, p, q, error);
  };
  std::shared_ptr<Fiver> fiver_;
};

class BigwigIdx final : public Idx {
public:
  static std::shared_ptr<Idx>
  make(const std::vector<std::shared_ptr<Warren>> &warrens) {
    std::shared_ptr<BigwigIdx> idx =
        std::shared_ptr<BigwigIdx>(new BigwigIdx());
    idx->warrens_ = warrens;
    return idx;
  };

  virtual ~BigwigIdx(){};
  BigwigIdx(const BigwigIdx &) = delete;
  BigwigIdx &operator=(const BigwigIdx &) = delete;
  BigwigIdx(BigwigIdx &&) = delete;
  BigwigIdx &operator=(BigwigIdx &&) = delete;

private:
  BigwigIdx(){};
  std::string recipe_() final { return ""; };
  std::unique_ptr<Hopper> hopper_(addr feature) final {
    std::unique_ptr<cottontail::Hopper> hopper = nullptr;
    for (size_t i = warrens_.size(); i > 0; --i)
      if (warrens_[i - 1] != nullptr) {
        if (hopper == nullptr)
          hopper = warrens_[i - 1]->idx()->hopper(feature);
        else
          hopper = std::make_unique<gcl::Merge>(
              std::move(warrens_[i - 1]->idx()->hopper(feature)),
              std::move(hopper));
      }
    if (hopper == nullptr)
      return std::make_unique<EmptyHopper>();
    else
      return hopper;
  };
  addr count_(addr feature) final {
    addr n = 0;
    for (auto &warren : warrens_)
      if (warren != nullptr)
        n += warren->idx()->count(feature);
    return n;
  }
  addr vocab_() final {
    // TODO
    // Technically wrong, but fixable with some work
    // Really intended to be an approximation anyway
    addr n = 0;
    for (auto &warren : warrens_)
      if (warren != nullptr)
        n += warren->idx()->vocab();
    return n;
  }
  std::vector<std::shared_ptr<Warren>> warrens_;
};

class BigwigTxt final : public Txt {
public:
  static std::shared_ptr<Txt>
  make(const std::vector<std::shared_ptr<Warren>> &warrens) {
    std::shared_ptr<BigwigTxt> txt =
        std::shared_ptr<BigwigTxt>(new BigwigTxt());
    txt->warrens_ = warrens;
    return txt;
  };

  virtual ~BigwigTxt(){};
  BigwigTxt(const BigwigTxt &) = delete;
  BigwigTxt &operator=(const BigwigTxt &) = delete;
  BigwigTxt(BigwigTxt &&) = delete;
  BigwigTxt &operator=(BigwigTxt &&) = delete;

private:
  BigwigTxt(){};
  std::string recipe_() final { return ""; };
  std::string translate_(addr p, addr q) final {
    std::string result;
    for (auto &warren : warrens_)
      result += warren->txt()->translate(p, q);
    return result;
  };
  addr tokens_() final {
    addr n = 0;
    for (auto &warren : warrens_)
      n += warren->txt()->tokens();
    return n;
  };
  std::vector<std::shared_ptr<Warren>> warrens_;
};

std::shared_ptr<Bigwig>
Bigwig::make(std::shared_ptr<Working> working,
             std::shared_ptr<Featurizer> featurizer,
             std::shared_ptr<Tokenizer> tokenizer,
             std::shared_ptr<Fluffle> fluffle, std::string *error,
             std::shared_ptr<std::map<std::string, std::string>> parameters,
             std::shared_ptr<Compressor> posting_compressor,
             std::shared_ptr<Compressor> fvalue_compressor,
             std::shared_ptr<Compressor> text_compressor) {
  if (featurizer == nullptr) {
    safe_set(error) = "Bigwig needs a featurizer (got nullptr)";
    return nullptr;
  }
  if (tokenizer == nullptr) {
    safe_set(error) = "Bigwig needs a tokenizer (got nullptr)";
    return nullptr;
  }
  if (fluffle == nullptr) {
    safe_set(error) = "Bigwig needs a fluffle (got nullptr)";
    return nullptr;
  }
  std::shared_ptr<Bigwig> bigwig = std::shared_ptr<Bigwig>(
      new Bigwig(working, featurizer, tokenizer, nullptr, nullptr));
  bigwig->parameters_ = parameters;
  bigwig->fiver_ = nullptr;
  bigwig->fluffle_ = fluffle;
  std::shared_ptr<Compressor> null_compressor = nullptr;
  if (posting_compressor == nullptr || fvalue_compressor == nullptr ||
      text_compressor == nullptr)
    null_compressor = Compressor::make("null", "");
  if (posting_compressor == nullptr)
    bigwig->posting_compressor_ = null_compressor;
  else
    bigwig->posting_compressor_ = posting_compressor;
  if (fvalue_compressor == nullptr)
    bigwig->fvalue_compressor_ = null_compressor;
  else
    bigwig->fvalue_compressor_ = fvalue_compressor;
  if (text_compressor == nullptr)
    bigwig->text_compressor_ = null_compressor;
  else
    bigwig->text_compressor_ = text_compressor;
  return bigwig;
}

void Bigwig::start_() {
  fluffle_->lock.lock();
  warrens_ = fluffle_->warrens;
  idx_ = BigwigIdx::make(warrens_);
  assert(idx_ != nullptr);
  txt_ = BigwigTxt::make(warrens_);
  assert(txt_ != nullptr);
  fluffle_->lock.unlock();
}

void Bigwig::end_() {
  warrens_.clear();
  idx_ = nullptr;
  txt_ = nullptr;
}

bool Bigwig::transaction_(std::string *error) {
  fiver_ =
      Fiver::make(working_, featurizer_, tokenizer_, error, parameters_,
                  posting_compressor_, fvalue_compressor_, text_compressor_);
  if (fiver_ == nullptr)
    return false;
  annotator_ = BigwigAnnotator::make(fiver_, error);
  if (annotator_ == nullptr) {
    fiver_ = nullptr;
    return false;
  }
  appender_ = BigwigAppender::make(fiver_, error);
  if (appender_ == nullptr) {
    annotator_ = nullptr;
    fiver_ = nullptr;
    return false;
  }
  if (!fiver_->transaction(error)) {
    appender_ = nullptr;
    annotator_ = nullptr;
    fiver_ = nullptr;
    return false;
  }
  return true;
}

bool Bigwig::ready_() {
    fluffle_->lock.lock();
    fluffle_->address = fiver_->relocate(fluffle_->address);
    fluffle_->warrens.push_back(fiver_);
    fiver_->start();
    fluffle_->lock.unlock();
    return fiver_->ready();
}

void Bigwig::commit_() {
  fluffle_->lock.lock();
  fiver_->commit();
  fiver_->end();
  fiver_->start();
  fluffle_->lock.unlock();
  appender_ = nullptr;
  annotator_ = nullptr;
  fiver_ = nullptr;
}

void Bigwig::abort_() {
  fiver_->abort();
  appender_ = nullptr;
  annotator_ = nullptr;
  fiver_ = nullptr;
}
} // namespace cottontail
