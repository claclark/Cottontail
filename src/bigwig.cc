#include "src/bigwig.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "src/annotator.h"
#include "src/appender.h"
#include "src/compressor.h"
#include "src/core.h"
#include "src/featurizer.h"
#include "src/fluffle.h"
#include "src/hopper.h"
#include "src/recipe.h"
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

std::shared_ptr<Bigwig> Bigwig::make(
    std::shared_ptr<Working> working, std::shared_ptr<Featurizer> featurizer,
    std::shared_ptr<Tokenizer> tokenizer, std::shared_ptr<Fluffle> fluffle,
    std::string *error, std::shared_ptr<Compressor> posting_compressor,
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
  std::shared_ptr<Bigwig> bigwig = std::shared_ptr<Bigwig>(
      new Bigwig(working, featurizer, tokenizer, nullptr, nullptr));
  bigwig->fiver_ = nullptr;
  if (fluffle == nullptr)
    bigwig->fluffle_ = Fluffle::make();
  else
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

std::shared_ptr<Warren> Bigwig::clone_(std::string *error) {
  std::shared_ptr<Warren> bigwig =
      Bigwig::make(working_, featurizer_, tokenizer_, fluffle_, error,
                   posting_compressor_, fvalue_compressor_, text_compressor_);
  if (bigwig == nullptr)
    return nullptr;
  std::shared_ptr<cottontail::Stemmer> the_stemmer =
      cottontail::Stemmer::make(stemmer_->name(), stemmer_->recipe(), error);
  if (the_stemmer == nullptr)
    return nullptr;
  if (!bigwig->set_stemmer(the_stemmer, error))
    return nullptr;
  return bigwig;
}

void Bigwig::start_() {
  fluffle_->lock.lock();
  for (auto &warren : fluffle_->warrens)
    if (warren->name() == "fiver")
      warrens_.push_back(warren);
  fluffle_->lock.unlock();
  idx_ = BigwigIdx::make(warrens_);
  assert(idx_ != nullptr);
  txt_ = BigwigTxt::make(warrens_);
  assert(txt_ != nullptr);
}

void Bigwig::end_() {
  warrens_.clear();
  idx_ = nullptr;
  txt_ = nullptr;
}

bool Bigwig::set_parameter_(const std::string &key, const std::string &value,
                            std::string *error) {
  std::shared_ptr<std::map<std::string, std::string>> parameters =
      std::make_shared<std::map<std::string, std::string>>();
  fluffle_->lock.lock();
  if (working_ != nullptr && !set_parameter_in_dna(working_, key, value, error))
    return false;
  if (fluffle_->parameters != nullptr)
    parameters = fluffle_->parameters;
  else
    fluffle_->parameters = parameters;
  (*parameters)[key] = value;
  fluffle_->lock.unlock();
  return true;
}

bool Bigwig::get_parameter_(const std::string &key, std::string *value,
                            std::string *error) {
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
}

bool Bigwig::transaction_(std::string *error) {
  fluffle_->lock.lock();
  std::shared_ptr<std::map<std::string, std::string>> parameters =
      fluffle_->parameters;
  fluffle_->lock.unlock();
  fiver_ =
      Fiver::make(working_, featurizer_, tokenizer_, error, posting_compressor_,
                  fvalue_compressor_, text_compressor_);
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
  fiver_->sequence(fluffle_->sequence);
  fluffle_->sequence++;
  fluffle_->warrens.push_back(fiver_);
  fluffle_->lock.unlock();
  return fiver_->ready();
}

namespace {
void merge_worker(std::shared_ptr<Fluffle> fluffle) {
  for (;;) {
    fluffle->lock.lock();
    size_t start, end;
    for (start = 0; start < fluffle->warrens.size(); start++)
      if (fluffle->warrens[start]->name() == "fiver" ||
          fluffle->warrens[start]->name() == "remove")
        break;
    if (start == fluffle->warrens.size()) {
      fluffle->merging = false;
      fluffle->lock.unlock();
      return;
    }
    for (end = start + 1; end < fluffle->warrens.size(); end++)
      if (fluffle->warrens[end]->name() != "fiver" &&
          fluffle->warrens[end]->name() != "remove")
        break;
    if (end - start < 2) {
      fluffle->merging = false;
      fluffle->lock.unlock();
      return;
    }
    std::vector<std::shared_ptr<Fiver>> fivers;
    for (size_t i = start; i < end; i++)
      if (fluffle->warrens[i]->name() == "fiver")
        fivers.push_back(std::static_pointer_cast<Fiver>(fluffle->warrens[i]));
    fluffle->lock.unlock();
    std::shared_ptr<Fiver> merged = Fiver::merge(fivers);
    fluffle->lock.lock();
    merged->pickle();
    for (auto &fiver : fivers)
      fiver->discard();
    std::vector<std::shared_ptr<Warren>> warrens;
    for (size_t i = 0; i < start; i++)
      warrens.push_back(fluffle->warrens[i]);
    if (merged != nullptr) {
      warrens.push_back(merged);
      merged->start();
    }
    for (size_t i = end; i < fluffle->warrens.size(); i++)
      warrens.push_back(fluffle->warrens[i]);
    fluffle->warrens = warrens;
    fluffle->lock.unlock();
  }
}

void try_merge(std::shared_ptr<Fluffle> fluffle) {
  fluffle->lock.lock();
  if (!fluffle->merging) {
    fluffle->merging = true;
    std::thread t(merge_worker, fluffle);
    t.detach();
  }
  fluffle->lock.unlock();
}
} // namespace

void Bigwig::commit_() {
  fluffle_->lock.lock();
  fiver_->commit();
  fiver_->start();
  fluffle_->lock.unlock();
  if (merge_)
    try_merge(fluffle_);
  appender_ = nullptr;
  annotator_ = nullptr;
  fiver_ = nullptr;
}

void Bigwig::abort_() {
  fluffle_->lock.lock();
  fiver_->abort();
  fiver_->start();
  fluffle_->lock.unlock();
  if (merge_)
    try_merge(fluffle_);
  appender_ = nullptr;
  annotator_ = nullptr;
  fiver_ = nullptr;
}

std::string Bigwig::recipe_() {
  std::map<std::string, std::string> warren_parameters;
  warren_parameters["warren"] = name();
  if (tokenizer() != nullptr) {
    std::map<std::string, std::string> tokenizer_parameters;
    tokenizer_parameters["name"] = tokenizer_->name();
    tokenizer_parameters["recipe"] = tokenizer_->recipe();
    warren_parameters["tokenizer"] = freeze(tokenizer_parameters);
  }
  if (featurizer() != nullptr) {
    std::map<std::string, std::string> featurizer_parameters;
    featurizer_parameters["name"] = featurizer()->name();
    featurizer_parameters["recipe"] = featurizer()->recipe();
    warren_parameters["featurizer"] = freeze(featurizer_parameters);
  }
  {
    std::map<std::string, std::string> txt_parameters;
    txt_parameters["name"] = "bigwig";
    {
      std::map<std::string, std::string> txt_recipe_parameters;
      if (text_compressor_ != nullptr) {
        txt_recipe_parameters["compressor"] = text_compressor_->name();
        txt_recipe_parameters["compressor_recipe"] = text_compressor_->recipe();
      }
      txt_parameters["recipe"] = freeze(txt_recipe_parameters);
    }
    warren_parameters["txt"] = freeze(txt_parameters);
  }
  {
    std::map<std::string, std::string> idx_parameters;
    idx_parameters["name"] = "bigwig";
    {
      std::map<std::string, std::string> idx_recipe_parameters;
      if (fvalue_compressor_ != nullptr) {
        idx_recipe_parameters["fvalue_compressor"] = fvalue_compressor_->name();
        idx_recipe_parameters["fvalue_compressor_recipe"] =
            fvalue_compressor_->recipe();
      }
      if (posting_compressor_ != nullptr) {
        idx_recipe_parameters["posting_compressor"] =
            posting_compressor_->name();
        idx_recipe_parameters["posting_compressor_recipe"] =
            posting_compressor_->recipe();
      }
      idx_parameters["recipe"] = freeze(idx_recipe_parameters);
    }
    warren_parameters["idx"] = freeze(idx_parameters);
  }
  fluffle_->lock.lock();
  std::map<std::string, std::string> extra_parameters = *(fluffle_->parameters);
  fluffle_->lock.unlock();
  warren_parameters["parameters"] = freeze(extra_parameters);
  return freeze(warren_parameters);
}

} // namespace cottontail
