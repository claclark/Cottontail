#include "src/fiver.h"

#include <algorithm>
#include <iostream>
#include <map>

#include "src/annotator.h"
#include "src/appender.h"
#include "src/compressor.h"
#include "src/core.h"
#include "src/fastid_txt.h"
#include "src/featurizer.h"
#include "src/hopper.h"
#include "src/recipe.h"
#include "src/simple.h"
#include "src/simple_posting.h"
#include "src/stemmer.h"
#include "src/tokenizer.h"

namespace cottontail {

class FiverAnnotator : public Annotator {
public:
  static std::shared_ptr<Annotator>
  make(std::shared_ptr<std::vector<Annotation>> annotations,
       std::string *error = nullptr) {
    if (annotations == nullptr) {
      safe_set(error) = "FiverAnnotator got null annotation vector";
      return nullptr;
    }
    std::shared_ptr<FiverAnnotator> annotator =
        std::shared_ptr<FiverAnnotator>(new FiverAnnotator());
    annotator->annotations_ = annotations;
    return annotator;
  };

  virtual ~FiverAnnotator(){};
  FiverAnnotator(const FiverAnnotator &) = delete;
  FiverAnnotator &operator=(const FiverAnnotator &) = delete;
  FiverAnnotator(FiverAnnotator &&) = delete;
  FiverAnnotator &operator=(FiverAnnotator &&) = delete;

private:
  FiverAnnotator(){};
  std::string recipe_() final { return ""; };
  bool annotate_(addr feature, addr p, addr q, fval v,
                 std::string *error = nullptr) final {
    annotations_->emplace_back(feature, p, q, v);
    return true;
  };
  std::shared_ptr<std::vector<Annotation>> annotations_;
};

class FiverAppender : public Appender {
public:
  static std::shared_ptr<Appender>
  make(std::shared_ptr<std::vector<std::string>> appends,
       std::shared_ptr<Featurizer> featurizer,
       std::shared_ptr<Tokenizer> tokenizer,
       std::shared_ptr<Annotator> annotator, std::string *error = nullptr) {
    if (appends == nullptr) {
      safe_set(error) = "FiverAnnotator got null append vector";
      return nullptr;
    }
    std::shared_ptr<FiverAppender> appender =
        std::shared_ptr<FiverAppender>(new FiverAppender());
    appender->appends_ = appends;
    appender->address_ = 0;
    appender->feature_ = featurizer->featurize("\n"); // bad magic
    appender->featurizer_ = featurizer;
    appender->tokenizer_ = tokenizer;
    appender->annotator_ = annotator;
    return appender;
  };

  virtual ~FiverAppender(){};
  FiverAppender(const FiverAppender &) = delete;
  FiverAppender &operator=(const FiverAppender &) = delete;
  FiverAppender(FiverAppender &&) = delete;
  FiverAppender &operator=(FiverAppender &&) = delete;

private:
  FiverAppender(){};
  std::string recipe_() final { return ""; };
  bool append_(const std::string &text, addr *p, addr *q, std::string *error) {
    appends_->push_back(text);
    std::vector<Token> tokens = tokenizer_->tokenize(featurizer_, text);
    if (tokens.size() == 0) {
      *p = address_ + 1;
      *q = address_;
      return true;
    }
    addr last_address = address_ + tokens[tokens.size() - 1].address;
    for (auto &token : tokens) {
      token.address += address_;
      if (!annotator_->annotate(token.feature, token.address, token.address,
                                error))
        return false;
    }
    *p = address_;
    *q = last_address;
    address_ = (last_address + 1);
    addr v = appends_->size() - 1;
    return annotator_->annotate(feature_, *p, *q, v, error);
  }
  addr address_;
  addr feature_;
  std::shared_ptr<std::vector<std::string>> appends_;
  std::shared_ptr<Featurizer> featurizer_;
  std::shared_ptr<Tokenizer> tokenizer_;
  std::shared_ptr<Annotator> annotator_;
};

class FiverIdx final : public Idx {
public:
  static std::shared_ptr<Idx>
  make(std::shared_ptr<std::map<addr, std::shared_ptr<SimplePosting>>> index,
       std::string *error = nullptr) {
    if (index == nullptr) {
      safe_set(error) = "FiverIdx got null index";
      return nullptr;
    }
    std::shared_ptr<FiverIdx> idx = std::shared_ptr<FiverIdx>(new FiverIdx());
    idx->index_ = index;
    return idx;
  };

  virtual ~FiverIdx(){};
  FiverIdx(const FiverIdx &) = delete;
  FiverIdx &operator=(const FiverIdx &) = delete;
  FiverIdx(FiverIdx &&) = delete;
  FiverIdx &operator=(FiverIdx &&) = delete;

private:
  FiverIdx(){};
  std::string recipe_() final { return ""; };
  std::unique_ptr<Hopper> hopper_(addr feature) final {
    auto posting = index_->find(feature);
    if (posting == index_->end())
      return std::make_unique<EmptyHopper>();
    else
      return posting->second->hopper();
  };
  addr count_(addr feature) final {
    auto posting = index_->find(feature);
    if (posting == index_->end())
      return 0;
    else
      return posting->second->size();
  };
  addr vocab_() final { return index_->size(); };
  void reset_() final{};
  std::shared_ptr<std::map<addr, std::shared_ptr<SimplePosting>>> index_;
};

std::shared_ptr<Fiver> Fiver::make(
    std::shared_ptr<Working> working, std::shared_ptr<Featurizer> featurizer,
    std::shared_ptr<Tokenizer> tokenizer, addr identifier, std::string *error,
    std::shared_ptr<std::map<std::string, std::string>> parameters,
    std::shared_ptr<Compressor> posting_compressor,
    std::shared_ptr<Compressor> fvalue_compressor,
    std::shared_ptr<Compressor> text_compressor) {
  if (featurizer == nullptr) {
    safe_set(error) = "Fiver needs a featurizer (got nullptr)";
    return nullptr;
  }
  if (tokenizer == nullptr) {
    safe_set(error) = "Fiver needs a tokenizer (got nullptr)";
    return nullptr;
  }
  std::shared_ptr<std::vector<std::string>> appends =
      std::make_shared<std::vector<std::string>>();
  std::shared_ptr<std::vector<Annotation>> annotations =
      std::make_shared<std::vector<Annotation>>();
  std::shared_ptr<std::map<addr, std::shared_ptr<SimplePosting>>> index =
      std::make_shared<std::map<addr, std::shared_ptr<SimplePosting>>>();
  std::shared_ptr<Annotator> annotator =
      FiverAnnotator::make(annotations, error);
  if (annotator == nullptr)
    return nullptr;
  std::shared_ptr<Appender> appender =
      FiverAppender::make(appends, featurizer, tokenizer, annotator, error);
  if (annotator == nullptr)
    return nullptr;
  std::shared_ptr<Idx> idx = FiverIdx::make(index, error);
  if (idx == nullptr)
    return nullptr;
  std::shared_ptr<Fiver> fiver = std::shared_ptr<Fiver>(
      new Fiver(working, featurizer, tokenizer, idx, nullptr));
  if (fiver == nullptr)
    return nullptr;
  fiver->parameters_ = parameters;
  std::shared_ptr<Compressor> null_compressor = nullptr;
  if (posting_compressor == nullptr || fvalue_compressor == nullptr ||
      text_compressor == nullptr)
    null_compressor = Compressor::make("null", "");
  if (posting_compressor == nullptr)
    fiver->posting_compressor_ = null_compressor;
  else
    fiver->posting_compressor_ = posting_compressor;
  if (fvalue_compressor == nullptr)
    fiver->fvalue_compressor_ = null_compressor;
  else
    fiver->fvalue_compressor_ = fvalue_compressor;
  if (text_compressor == nullptr)
    fiver->text_compressor_ = null_compressor;
  else
    fiver->text_compressor_ = text_compressor;
  fiver->annotator_ = annotator;
  fiver->appender_ = appender;
  fiver->built_ = false;
  return fiver;
}

bool Fiver::transaction_(std::string *error = nullptr) {
  if (built_) {
    safe_set(error) = "Fiver does not allow additional transactions";
    return false;
  }
  return true;
};

bool Fiver::ready_() { return false; };

void Fiver::commit_() {
  std::sort(annotations_->begin(), annotations_->end(),
            [](const Annotation &a, const Annotation &b) {
              return a.feature < b.feature ||
                     (a.feature == b.feature && a.p < b.p);
            });
  std::shared_ptr<SimplePostingFactory> posting_factory =
      SimplePostingFactory::make(posting_compressor_, fvalue_compressor_);
  std::vector<Annotation>::iterator it = annotations_->begin();
  while (it < annotations_->end()) {
    std::shared_ptr<SimplePosting> posting =
        posting_factory->posting_from_annotations(&it, annotations_->end());
    (*index_)[posting->feature()] = posting;
  }
  annotations_->clear();
  built_ = true;
};

void Fiver::abort_() {
  appends_->clear();
  annotations_->clear();
};

} // namespace cottontail
