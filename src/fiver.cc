#include "src/fiver.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <map>

#include "src/annotator.h"
#include "src/appender.h"
#include "src/compressor.h"
#include "src/core.h"
#include "src/fastid_txt.h"
#include "src/featurizer.h"
#include "src/hopper.h"
#include "src/null_idx.h"
#include "src/null_txt.h"
#include "src/recipe.h"
#include "src/simple.h"
#include "src/simple_posting.h"
#include "src/stemmer.h"
#include "src/tokenizer.h"

namespace cottontail {

constexpr addr staging = maxfinity / 2;

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
  bool transaction_(std::string *error) final {
    if (annotations_ == nullptr) {
      safe_set(error) =
          "FiverAnnotator does not support more than one transaction";
      return false;
    }
    return true;
  };
  bool ready_() final { return true; };
  void commit_() final { annotations_ = nullptr; };
  void abort_() final { annotations_->clear(); };
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
    appends->push_back("");
    std::shared_ptr<FiverAppender> appender =
        std::shared_ptr<FiverAppender>(new FiverAppender());
    appender->appends_ = appends;
    appender->address_ = staging;
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
    if ((*appends_)[appends_->size() - 1].length() > 0)
      (*appends_)[appends_->size() - 1] += "\n";
    (*appends_)[appends_->size() - 1] += text;
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
    return true;
  }
  bool transaction_(std::string *error) final {
    if (appends_ == nullptr) {
      safe_set(error) =
          "FiverAppender does not support more than one transaction";
      return false;
    }
    return true;
  };
  bool ready_() { return true; };
  void commit_() {
    if (address_ > staging) {
      addr separator = featurizer_->featurize("\n");
      assert(annotator_->annotate(separator, staging, address_ - 1, (addr)0));
      address_ = staging;
    }
    appends_ = nullptr;
  }
  void abort_() {
    address_ = staging;
    appends_->clear();
  }
  addr address_;
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
  std::shared_ptr<std::map<addr, std::shared_ptr<SimplePosting>>> index_;
};

class FiverTxt final : public Txt {
public:
  static std::shared_ptr<Txt>
  make(std::shared_ptr<Featurizer> featurizer,
       std::shared_ptr<Tokenizer> tokenizer, std::shared_ptr<Idx> idx,
       std::shared_ptr<std::vector<std::string>> text,
       std::string *error = nullptr) {
    if (idx == nullptr) {
      safe_set(error) = "FiverIdx got null idx";
      return nullptr;
    }
    if (text == nullptr) {
      safe_set(error) = "FiverIdx got null text vector";
      return nullptr;
    }
    std::shared_ptr<FiverTxt> txt = std::shared_ptr<FiverTxt>(new FiverTxt());
    txt->count_ = -1;
    txt->tokenizer_ = tokenizer;
    txt->hopper_ = idx->hopper(featurizer->featurize("\n"));
    txt->text_ = text;
    return txt;
  };

  virtual ~FiverTxt(){};
  FiverTxt(const FiverTxt &) = delete;
  FiverTxt &operator=(const FiverTxt &) = delete;
  FiverTxt(FiverTxt &&) = delete;
  FiverTxt &operator=(FiverTxt &&) = delete;

private:
  FiverTxt(){};
  std::string recipe_() final { return ""; };
  std::string translate_(addr p, addr q) final {
    addr p0, q0, i0;
    hopper_->rho(p, &p0, &q0, &i0);
    if (p0 > q)
      return "";
    const char *s = (*text_)[i0].c_str();
    size_t l = (*text_)[i0].length();
    const char *t;
    addr ps;
    if (p > p0) {
      ps = p;
      t = tokenizer_->skip(s, l, p - p0);
      l -= t - s;
    } else {
      ps = p0;
      t = s;
    }
    if (q < q0) {
      const char *e = tokenizer_->skip(t, l, q - ps + 1);
      l = e - t;
    }
    std::string result = (*text_)[i0].substr(t - s, l);
    if (q <= q0)
      return result;
    addr p1, q1, i1;
    hopper_->ohr(q, &p1, &q1, &i1);
    if (p1 == p0)
      return result;
    for (addr j = i0 + 1; j < i1; j++)
      result += (*text_)[j];
    if (q1 <= q) {
      result += (*text_)[i1];
    } else {
      const char *s = (*text_)[i1].c_str();
      const char *e = tokenizer_->skip(s, (*text_)[i1].length(), q1 - p1 + 1);
      result += (*text_)[i1].substr(0, e - s);
    }
    return result;
  };
  addr tokens_() final {
    if (count_ >= 0)
      return count_;
    count_ = 0;
    addr p, q;
    for (hopper_->tau(0, &p, &q); p < maxfinity; hopper_->tau(p + 1, &p, &q))
      count_ += q - p + 1;
    return count_;
  }
  addr count_;
  std::shared_ptr<Tokenizer> tokenizer_;
  std::unique_ptr<Hopper> hopper_;
  std::shared_ptr<std::vector<std::string>> text_;
};

std::shared_ptr<Fiver>
Fiver::make(std::shared_ptr<Working> working,
            std::shared_ptr<Featurizer> featurizer,
            std::shared_ptr<Tokenizer> tokenizer, std::string *error,
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
  std::shared_ptr<Idx> idx = NullIdx::make("", error);
  if (idx == nullptr)
    return nullptr;
  std::shared_ptr<Txt> txt = NullTxt::make("", error);
  if (txt == nullptr)
    return nullptr;
  std::shared_ptr<Fiver> fiver = std::shared_ptr<Fiver>(
      new Fiver(working, featurizer, tokenizer, idx, txt));
  if (fiver == nullptr)
    return nullptr;
  fiver->built_ = false;
  fiver->where_ = 0;
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
  fiver->appends_ = appends;
  fiver->annotations_ = annotations;
  fiver->index_ = index;
  fiver->annotator_ = annotator;
  fiver->appender_ = appender;
  return fiver;
}

bool Fiver::transaction_(std::string *error = nullptr) {
  if (built_) {
    safe_set(error) = "Fiver does not support more than one transaction";
    return false;
  }
  if (!appender_->transaction(error))
    return false;
  if (!annotator_->transaction(error)) {
    appender_->abort();
    return false;
  }
  return true;
};

bool Fiver::ready_() { return appender_->ready() && annotator_->ready(); };

void Fiver::commit_() {
  appender_->commit();
  annotator_->commit();
  if (annotations_->size() == 0)
    return;
  addr relocate = staging + where_;
  for (auto &annotation : (*annotations_))
    if (annotation.p >= staging) {
      annotation.p -= relocate;
      annotation.q -= relocate;
    }
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
  idx_ = FiverIdx::make(index_);
  assert(idx_ != nullptr);
  txt_ = FiverTxt::make(featurizer_, tokenizer_, idx_, appends_);
  assert(txt_ != nullptr);
  built_ = true;
};

void Fiver::abort_() {
  appender_->abort();
  annotator_->abort();
  where_ = 0;
};

} // namespace cottontail
