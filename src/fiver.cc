#include "src/fiver.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>

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
const std::string separator = "\n";

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
  static std::shared_ptr<Appender> make(std::shared_ptr<std::string> text,
                                        std::shared_ptr<Featurizer> featurizer,
                                        std::shared_ptr<Tokenizer> tokenizer,
                                        std::shared_ptr<Annotator> annotator,
                                        std::string *error = nullptr) {
    if (text == nullptr || featurizer == nullptr || tokenizer == nullptr ||
        annotator == nullptr) {
      safe_set(error) = "FiverAnnotator got null pointer";
      return nullptr;
    }
    std::shared_ptr<FiverAppender> appender =
        std::shared_ptr<FiverAppender>(new FiverAppender());
    appender->text_ = text;
    appender->address_ = staging;
    appender->featurizer_ = featurizer;
    appender->tokenizer_ = tokenizer;
    appender->annotator_ = annotator;
    return appender;
  };
  addr appended() { return address_ - staging; };

  virtual ~FiverAppender(){};
  FiverAppender(const FiverAppender &) = delete;
  FiverAppender &operator=(const FiverAppender &) = delete;
  FiverAppender(FiverAppender &&) = delete;
  FiverAppender &operator=(FiverAppender &&) = delete;

private:
  FiverAppender(){};
  std::string recipe_() final { return ""; };
  bool append_(const std::string &text, addr *p, addr *q, std::string *error) {
    if (text.length() == 0) {
      *p = address_ + 1;
      *q = address_;
      return true;
    }
    if (text_->length() > 0 && text_->back() != '\n')
      *text_ += "\n";
    *text_ += text;
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
    if (text_ == nullptr) {
      safe_set(error) =
          "FiverAppender does not support more than one transaction";
      return false;
    }
    return true;
  };
  bool ready_() {
    if (text_->length() > 0 && text_->back() != '\n')
      *text_ += "\n";
    if (address_ > staging)
      assert(annotator_->annotate(featurizer_->featurize(separator), staging,
                                  address_ - 1, (addr)0));
    return true;
  }
  void commit_() {
    address_ = staging;
    text_ = nullptr;
  }
  void abort_() {
    address_ = staging;
    (*text_) = "";
  }
  addr address_;
  std::shared_ptr<std::string> text_;
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
  static std::shared_ptr<Txt> make(std::shared_ptr<Featurizer> featurizer,
                                   std::shared_ptr<Tokenizer> tokenizer,
                                   std::shared_ptr<Idx> idx,
                                   std::shared_ptr<std::string> text,
                                   std::string *error = nullptr) {
    if (idx == nullptr) {
      safe_set(error) = "FiverIdx got null idx";
      return nullptr;
    }
    if (text == nullptr) {
      safe_set(error) = "FiverIdx got null text pointer";
      return nullptr;
    }
    std::shared_ptr<FiverTxt> txt = std::shared_ptr<FiverTxt>(new FiverTxt());
    txt->count_ = -1;
    txt->tokenizer_ = tokenizer;
    txt->hopper_ = idx->hopper(featurizer->featurize(separator));
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
    if (p < 0)
      p = 0;
    if (p == maxfinity || q < p)
      return "";
    addr p0, q0, i;
    addr p1, q1, j;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      hopper_->rho(p, &p0, &q0, &i);
      if (p0 == maxfinity || p0 > q)
        return "";
      hopper_->ohr(q, &p1, &q1, &j);
    }
    const char *t = text_->c_str();
    const char *s = t + i;
    if (p0 < p)
      s = tokenizer_->skip(s, text_->length() - (s - t), p - p0);
    if (q1 < q)
      return text_->substr(s - t, text_->length() - (s - t));
    const char *e = t + j;
    e = tokenizer_->skip(e, text_->length() - (e - t), q - p1 + 1);
    return text_->substr(s - t, e - s);
  };
  addr tokens_() final {
    std::lock_guard<std::mutex> lock(mutex_);
    if (count_ >= 0)
      return count_;
    count_ = 0;
    addr p, q;
    for (hopper_->tau(0, &p, &q); p < maxfinity; hopper_->tau(p + 1, &p, &q))
      count_ += q - p + 1;
    return count_;
  }
  bool range_(addr *p, addr *q) {
    std::lock_guard<std::mutex> lock(mutex_);
    hopper_->tau(minfinity + 1, p, q);
    if (*p == maxfinity)
      return false;
    addr r;
    hopper_->uat(maxfinity - 1, &r, q);
    return true;
  }
  addr count_;
  std::mutex mutex_;
  std::shared_ptr<Tokenizer> tokenizer_;
  std::unique_ptr<Hopper> hopper_;
  std::shared_ptr<std::string> text_;
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
  std::shared_ptr<std::string> text = std::make_shared<std::string>();
  std::shared_ptr<std::vector<Annotation>> annotations =
      std::make_shared<std::vector<Annotation>>();
  std::shared_ptr<std::map<addr, std::shared_ptr<SimplePosting>>> index =
      std::make_shared<std::map<addr, std::shared_ptr<SimplePosting>>>();
  std::shared_ptr<Annotator> annotator =
      FiverAnnotator::make(annotations, error);
  if (annotator == nullptr)
    return nullptr;
  std::shared_ptr<Appender> appender =
      FiverAppender::make(text, featurizer, tokenizer, annotator, error);
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
  fiver->text_ = text;
  fiver->annotations_ = annotations;
  fiver->index_ = index;
  fiver->annotator_ = annotator;
  fiver->appender_ = appender;
  return fiver;
}

std::shared_ptr<Fiver>
Fiver::merge(const std::vector<std::shared_ptr<Fiver>> &fivers,
             std::string *error, std::shared_ptr<Compressor> posting_compressor,
             std::shared_ptr<Compressor> fvalue_compressor,
             std::shared_ptr<Compressor> text_compressor) {
  if (fivers.size() == 0) {
    safe_set(error) = "Fiver::merge got empty vector";
    return nullptr;
  }
  if (fivers.size() == 1)
    return fivers[0];
  std::shared_ptr<std::string> text = std::make_shared<std::string>();
  std::vector<Annotation> ann;
  addr chunk = fivers[0]->featurizer_->featurize(separator);
  for (auto &fiver : fivers) {
    if (fiver->text_->length() > 0) {
      auto posting =
          fiver->index_->find(fiver->featurizer_->featurize(separator));
      if (posting != fiver->index_->end()) {
        std::unique_ptr<Hopper> hopper = posting->second->hopper();
        addr p, q, v;
        for (hopper->tau(0, &p, &q, &v); p < maxfinity;
             hopper->tau(p + 1, &p, &q, &v))
          ann.emplace_back(chunk, p, q, addr2fval(v + text->length()));
      }
      if (text->length() > 0 && text->back() != '\n')
        *text += "\n";
      *text += *(fiver->text_);
    }
  }
  std::sort(ann.begin(), ann.end(),
            [](const Annotation &a, const Annotation &b) {
              return a.feature < b.feature ||
                     (a.feature == b.feature &&
                      (a.q < b.q || (a.q == b.q && a.p > b.p)));
            });
  std::shared_ptr<Fiver> fiver = std::shared_ptr<Fiver>(
      new Fiver(fivers[0]->working_, fivers[0]->featurizer_,
                fivers[0]->tokenizer_, nullptr, nullptr));
  if (posting_compressor == nullptr)
    fiver->posting_compressor_ = fivers[0]->posting_compressor_;
  else
    fiver->posting_compressor_ = posting_compressor;
  if (fvalue_compressor == nullptr)
    fiver->fvalue_compressor_ = fivers[0]->fvalue_compressor_;
  else
    fiver->fvalue_compressor_ = fvalue_compressor;
  if (text_compressor == nullptr)
    fiver->text_compressor_ = fivers[0]->text_compressor_;
  else
    fiver->text_compressor_ = text_compressor;
  std::shared_ptr<SimplePostingFactory> posting_factory =
      SimplePostingFactory::make(fiver->posting_compressor_,
                                 fiver->fvalue_compressor_);
  std::shared_ptr<std::map<addr, std::shared_ptr<SimplePosting>>> index =
      std::make_shared<std::map<addr, std::shared_ptr<SimplePosting>>>();
  std::vector<Annotation>::iterator it = ann.begin();
  (*index)[chunk] = posting_factory->posting_from_annotations(&it, ann.end());
  std::map<addr, std::vector<std::shared_ptr<SimplePosting>>> unmerged_index;
  for (auto &f : fivers)
    for (auto &i : *(f->index_))
      if (i.first != chunk)
        unmerged_index[i.first].push_back(i.second);
  for (auto &u : unmerged_index)
    (*index)[u.first] = posting_factory->posting_from_merge(u.second);
  fiver->built_ = true;
  fiver->where_ = 0;
  fiver->name_ = "fiver";
  fiver->sequence_start_ = fivers[0]->sequence_start_;
  fiver->sequence_end_ = fivers[fivers.size() - 1]->sequence_end_;
  fiver->parameters_ = fivers[0]->parameters_;
  fiver->text_ = text;
  fiver->index_ = index;
  fiver->idx_ = FiverIdx::make(fiver->index_);
  fiver->txt_ = FiverTxt::make(fiver->featurizer_, fiver->tokenizer_,
                               fiver->idx_, fiver->text_);
  return fiver;
}

addr Fiver::relocate(addr where) {
  assert(!built_);
  where_ = where;
  std::shared_ptr<FiverAppender> fiver_appender =
      std::static_pointer_cast<FiverAppender>(appender_);
  return where + fiver_appender->appended();
};

void Fiver::sequence(addr number) { sequence_start_ = sequence_end_ = number; };

namespace {
std::string seq2str(addr sequence) {
  std::stringstream ss;
  ss.fill('0');
  ss.width(20);
  ss << sequence;
  return ss.str();
}
} // namespace

std::string Fiver::recipe_() {
  return seq2str(sequence_start_) + "." + seq2str(sequence_end_);
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

bool Fiver::ready_() {
  if (built_ || !appender_->ready() || !annotator_->ready())
    return false;
  if (annotations_->size() == 0)
    return true;
  index_->clear();
  addr relocate = staging - where_;
  for (auto &annotation : (*annotations_))
    if (annotation.p >= staging) {
      annotation.p -= relocate;
      annotation.q -= relocate;
    }
  std::sort(annotations_->begin(), annotations_->end(),
            [](const Annotation &a, const Annotation &b) {
              return a.feature < b.feature ||
                     (a.feature == b.feature &&
                      (a.q < b.q || (a.q == b.q && a.p > b.p)));
            });
  std::shared_ptr<SimplePostingFactory> posting_factory =
      SimplePostingFactory::make(posting_compressor_, fvalue_compressor_);
  std::vector<Annotation>::iterator it = annotations_->begin();
  while (it < annotations_->end()) {
    std::shared_ptr<SimplePosting> posting =
        posting_factory->posting_from_annotations(&it, annotations_->end());
    (*index_)[posting->feature()] = posting;
  }
  return true;
};

void Fiver::commit_() {
  if (!built_) {
    appender_->commit();
    annotator_->commit();
    annotations_->clear();
    idx_ = FiverIdx::make(index_);
    assert(idx_ != nullptr);
    txt_ = FiverTxt::make(featurizer_, tokenizer_, idx_, text_);
    assert(txt_ != nullptr);
    built_ = true;
    name_ = "fiver";
  }
};

void Fiver::abort_() {
  if (!built_) {
    appender_->abort();
    annotator_->abort();
    text_->clear();
    annotations_->clear();
    index_->clear();
    where_ = 0;
    built_ = true;
    name_ = "remove";
  }
};

bool Fiver::pickle(const std::string &filename, std::string *error) {
  if (!built_) {
    safe_set(error) = "Fiver must finish building before being pickled";
    return false;
  }
  if (text_compressor_->destructive()) {
    safe_set(error) = "Fiver's text compressor can't be destructive";
    return false;
  }
  std::string jarname;
  if (working() == nullptr)
    jarname = filename;
  else
    jarname = working()->make_name(filename);
  std::fstream jar;
  jar.open(jarname, std::ios::binary | std::ios::out);
  if (jar.fail()) {
    safe_set(error) = "Fiver can't create pickle jar: " + jarname;
    return false;
  }
  jar.write(reinterpret_cast<char *>(&sequence_start_),
            sizeof(sequence_start_));
  assert(!jar.fail());
  jar.write(reinterpret_cast<char *>(&sequence_end_), sizeof(sequence_end_));
  assert(!jar.fail());
  addr n = text_->length() + 1;
  jar.write(reinterpret_cast<char *>(&n), sizeof(n));
  addr m = n + text_compressor_->extra(n);
  std::unique_ptr<char[]> buffer = std::unique_ptr<char[]>(new char[m]);
  m = text_compressor_->crush((char *)(text_->c_str()), n, buffer.get(), m);
  jar.write(reinterpret_cast<char *>(&m), sizeof(m));
  assert(!jar.fail());
  jar.write(buffer.get(), m);
  assert(!jar.fail());
  for (auto &posting : *index_)
    posting.second->write(&jar);
  jar.close();
  return true;
}

std::shared_ptr<Fiver>
Fiver::unpickle(const std::string &filename, std::shared_ptr<Working> working,
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
  std::string jarname;
  if (working == nullptr)
    jarname = filename;
  else
    jarname = working->make_name(filename);
  std::fstream jar;
  jar.open(jarname, std::ios::binary | std::ios::in);
  if (jar.fail()) {
    safe_set(error) = "Fiver can't open pickle jar: " + jarname;
    return nullptr;
  }
  std::shared_ptr<Fiver> fiver = std::shared_ptr<Fiver>(
      new Fiver(working, featurizer, tokenizer, nullptr, nullptr));
  if (fiver == nullptr)
    return nullptr;
  fiver->built_ = true;
  fiver->where_ = 0;
  fiver->name_ = "fiver";
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
  jar.read(reinterpret_cast<char *>(&fiver->sequence_start_),
           sizeof(fiver->sequence_start_));
  assert(!jar.fail());
  jar.read(reinterpret_cast<char *>(&fiver->sequence_end_),
           sizeof(fiver->sequence_end_));
  assert(!jar.fail());
  addr n;
  jar.read(reinterpret_cast<char *>(&n), sizeof(n));
  assert(!jar.fail());
  n += fiver->text_compressor_->extra(n);
  std::unique_ptr<char[]> uncompressed = std::unique_ptr<char[]>(new char[n]);
  addr m;
  jar.read(reinterpret_cast<char *>(&m), sizeof(m));
  assert(!jar.fail());
  std::unique_ptr<char[]> compressed = std::unique_ptr<char[]>(new char[m]);
  jar.read(reinterpret_cast<char *>(compressed.get()), m);
  fiver->text_compressor_->tang(compressed.get(), m, uncompressed.get(), n);
  fiver->text_ = std::make_shared<std::string>(uncompressed.get());
  std::shared_ptr<SimplePostingFactory> factory = SimplePostingFactory::make(
      fiver->posting_compressor_, fiver->fvalue_compressor_);
  std::make_shared<std::map<addr, std::shared_ptr<SimplePosting>>>();
  fiver->index_ =
      std::make_shared<std::map<addr, std::shared_ptr<SimplePosting>>>();
  for (;;) {
    std::shared_ptr<SimplePosting> posting = factory->posting_from_file(&jar);
    if (posting == nullptr)
      break;
    (*fiver->index_)[posting->feature()] = posting;
  }
  jar.close();
  fiver->idx_ = FiverIdx::make(fiver->index_, error);
  if (fiver->idx_ == nullptr)
    return nullptr;
  fiver->txt_ =
      FiverTxt::make(featurizer, tokenizer, fiver->idx_, fiver->text_, error);
  if (fiver->txt_ == nullptr)
    return nullptr;
  return fiver;
}

} // namespace cottontail
