#include "src/fiver.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <unistd.h>
#include <vector>

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
#include "src/safe_map.h"
#include "src/simple.h"
#include "src/simple_posting.h"
#include "src/stemmer.h"
#include "src/tokenizer.h"
#include "src/vector_hopper.h"

namespace cottontail {

constexpr addr staging = maxfinity / 2;
const std::string transaction_tag = "\034"; // ASCII file separator
const std::string text_chunk_tag = "\035";  // ASCII group separator

class FiverAnnotator : public Annotator {
public:
  static std::shared_ptr<Annotator>
  make(std::shared_ptr<std::vector<Annotation>> annotations,
       std::string *error = nullptr) {
    if (annotations == nullptr) {
      safe_error(error) = "FiverAnnotator got null annotation vector";
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
  bool erase_(addr p, addr q, std::string *error) {
    annotations_->emplace_back(null_feature, p, q, 0.0);
    return true;
  }
  bool transaction_(std::string *error) final {
    if (annotations_ == nullptr) {
      safe_error(error) =
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
      safe_error(error) = "FiverAnnotator got null pointer";
      return nullptr;
    }
    std::shared_ptr<FiverAppender> appender =
        std::shared_ptr<FiverAppender>(new FiverAppender());
    appender->text_ = text;
    appender->first_address_ = appender->address_ = staging;
    appender->chunk_address_ = staging;
    appender->chunk_offset_ = 0;
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
    addr offset = text_->length();
    *text_ += text;
    if (!separator(text.back()))
      *text_ += "\n";
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
      if (token.address - chunk_address_ > CHUNK_SIZE) {
        if (!annotator_->annotate(featurizer_->featurize(text_chunk_tag),
                                  chunk_address_, token.address - 1,
                                  chunk_offset_))
          return false;
        chunk_address_ = token.address;
        chunk_offset_ = offset + token.offset;
      }
    }
    *p = address_;
    *q = last_address;
    address_ = (last_address + 1);
    return true;
  }
  bool transaction_(std::string *error) final {
    if (text_ == nullptr) {
      safe_error(error) =
          "FiverAppender does not support more than one transaction";
      return false;
    }
    return true;
  };
  bool ready_() {
    if (text_->length() > 0 && text_->back() != '\n')
      *text_ += "\n";
    if (address_ > first_address_)
      if (!annotator_->annotate(featurizer_->featurize(transaction_tag),
                                first_address_, address_ - 1))
        return false;
    if (address_ > chunk_address_)
      if (!annotator_->annotate(featurizer_->featurize(text_chunk_tag),
                                chunk_address_, address_ - 1, chunk_offset_))
        return false;
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
  static const addr CHUNK_SIZE = 512;
  addr address_;
  addr first_address_;
  addr chunk_address_;
  addr chunk_offset_;
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
      safe_error(error) = "FiverIdx got null index";
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
      safe_error(error) = "FiverIdx got null idx";
      return nullptr;
    }
    if (text == nullptr) {
      safe_error(error) = "FiverIdx got null text pointer";
      return nullptr;
    }
    std::shared_ptr<FiverTxt> txt = std::shared_ptr<FiverTxt>(new FiverTxt());
    txt->count_ = -1;
    txt->range_valid_ = false;
    txt->tokenizer_ = tokenizer;
    txt->hopper_ = idx->hopper(featurizer->featurize(text_chunk_tag));
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
  std::string name_() final { return "fiver"; };
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
    size_t n = text_->length();
    const char *t = text_->c_str();
    const char *s = t + i;
    if (p0 < p)
      s = tokenizer_->skip(s, n - i, p - p0);
    else
      p = p0;
    const char *e;
    if (q > q1)
      e = t + n;
    else if (q0 == q1)
      e = tokenizer_->skip(s, n - (s - t), q - p + 1);
    else
      e = tokenizer_->skip(t + j, n - j, q - p1 + 1);
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
    if (!range_valid_) {
      std::lock_guard<std::mutex> lock(mutex_);
      if (!range_valid_) {
        range_valid_ = true;
        hopper_->tau(minfinity + 1, &range_p_, &range_q_);
        if (range_p_ < maxfinity) {
          addr ignore;
          hopper_->uat(maxfinity - 1, &ignore, &range_q_);
        }
      }
    }
    *p = range_p_;
    *q = range_q_;
    return range_p_ < maxfinity;
  }
  addr count_;
  bool range_valid_;
  addr range_p_, range_q_;
  std::mutex mutex_;
  std::shared_ptr<Tokenizer> tokenizer_;
  std::unique_ptr<Hopper> hopper_;
  std::shared_ptr<std::string> text_;
};

std::shared_ptr<Fiver>
Fiver::make(std::shared_ptr<Working> working,
            std::shared_ptr<Featurizer> featurizer,
            std::shared_ptr<Tokenizer> tokenizer, std::string *error,
            std::shared_ptr<Compressor> posting_compressor,
            std::shared_ptr<Compressor> fvalue_compressor,
            std::shared_ptr<Compressor> text_compressor) {
  if (featurizer == nullptr) {
    safe_error(error) = "Fiver needs a featurizer (got nullptr)";
    return nullptr;
  }
  if (tokenizer == nullptr) {
    safe_error(error) = "Fiver needs a tokenizer (got nullptr)";
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
    safe_error(error) = "Fiver::merge got empty vector";
    return nullptr;
  }
  if (fivers.size() == 1)
    return fivers[0];
  std::shared_ptr<std::string> text = std::make_shared<std::string>();
  std::vector<Annotation> ann;
  addr chunk_feature = fivers[0]->featurizer_->featurize(text_chunk_tag);
  for (auto &fiver : fivers) {
    if (fiver->text_->length() > 0) {
      auto posting = fiver->index_->find(chunk_feature);
      if (posting != fiver->index_->end()) {
        if (text->length() > 0 && text->back() != '\n')
          *text += "\n";
        std::unique_ptr<Hopper> hopper = posting->second->hopper();
        addr p, q, v;
        for (hopper->tau(0, &p, &q, &v); p < maxfinity;
             hopper->tau(p + 1, &p, &q, &v))
          ann.emplace_back(chunk_feature, p, q, addr2fval(v + text->length()));
      }
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
  if (ann.size() > 0) {
    std::vector<Annotation>::iterator it = ann.begin();
    (*index)[chunk_feature] =
        posting_factory->posting_from_annotations(&it, ann.end());
  }
  std::shared_ptr<SimplePosting> exclude = nullptr;
  {
    std::vector<std::shared_ptr<SimplePosting>> x;
    for (auto &f : fivers) {
      auto posting = f->index_->find(null_feature);
      if (posting != f->index_->end())
        x.push_back(posting->second);
    }
    if (x.size() > 0) {
      exclude = posting_factory->posting_from_merge(x);
      if (exclude != nullptr)
        (*index)[null_feature] = exclude;
    }
  }
  std::map<addr, std::vector<std::shared_ptr<SimplePosting>>> unmerged_index;
  for (auto &f : fivers)
    for (auto &i : *(f->index_))
      if (i.first != null_feature && i.first != chunk_feature)
        unmerged_index[i.first].push_back(i.second);
  for (auto &u : unmerged_index) {
    std::shared_ptr<SimplePosting> p =
        posting_factory->posting_from_merge(u.second, exclude);
    if (p != nullptr)
      (*index)[u.first] = p;
  }
  fiver->built_ = true;
  fiver->where_ = 0;
  fiver->name_ = "fiver";
  fiver->storage_estimate_ = 0;
  for (auto &f : fivers)
    fiver->storage_estimate_ += f->storage_estimate_;
  fiver->sequence_start_ = fivers[0]->sequence_start_;
  fiver->sequence_end_ = fivers[fivers.size() - 1]->sequence_end_;
  fiver->text_ = text;
  fiver->index_ = index;
  fiver->idx_ = FiverIdx::make(fiver->index_);
  fiver->txt_ = FiverTxt::make(fiver->featurizer_, fiver->tokenizer_,
                               fiver->idx_, fiver->text_);
  return fiver;
}

std::unique_ptr<Hopper>
Fiver::merge(const std::vector<std::shared_ptr<Fiver>> &fivers, addr feature,
             std::string *error,
             SafeMap<addr, std::shared_ptr<SimplePosting>> *cache,
             std::shared_ptr<Compressor> posting_compressor,
             std::shared_ptr<Compressor> fvalue_compressor) {
  if (fivers.size() == 0)
    return std::make_unique<EmptyHopper>();
  if (fivers.size() == 1)
    return fivers[0]->idx()->hopper(feature);
  if (feature == fivers[0]->featurizer()->featurize(text_chunk_tag)) {
    std::vector<std::unique_ptr<cottontail::Hopper>> hoppers;
    for (size_t i = 0; i < fivers.size(); i++)
      if (fivers[i] != nullptr && fivers[i]->idx()->count(feature) > 0)
        hoppers.emplace_back(fivers[i]->idx()->hopper(feature));
    return gcl::VectorHopper::make(&hoppers, false, error);
  }
  std::shared_ptr<SimplePosting> posting;
  if (cache != nullptr && cache->try_get(feature, &posting))
    return posting->hopper();
  std::vector<std::shared_ptr<SimplePosting>> postings;
  for (size_t i = 0; i < fivers.size(); i++) {
    auto where = fivers[i]->index_->find(feature);
    if (where != fivers[i]->index_->end())
      postings.emplace_back(where->second);
  }
  if (postings.size() == 0)
    return std::make_unique<EmptyHopper>();
  std::shared_ptr<Compressor> the_posting_compressor = posting_compressor;
  if (the_posting_compressor == nullptr)
    the_posting_compressor = fivers[0]->posting_compressor_;
  std::shared_ptr<Compressor> the_fvalue_compressor = fvalue_compressor;
  if (the_fvalue_compressor == nullptr)
    the_fvalue_compressor = fivers[0]->fvalue_compressor_;
  std::shared_ptr<SimplePostingFactory> posting_factory =
      SimplePostingFactory::make(the_posting_compressor, the_fvalue_compressor);
  posting = posting_factory->posting_from_merge(postings);
  if (cache != nullptr)
    cache->set(feature, posting);
  return posting->hopper();
}

addr Fiver::relocate(addr where) {
  assert(!built_);
  where_ = where;
  std::shared_ptr<FiverAppender> fiver_appender =
      std::static_pointer_cast<FiverAppender>(appender_);
  return where + fiver_appender->appended();
};

void Fiver::set_sequence(addr number) {
  sequence_start_ = sequence_end_ = number;
};

void Fiver::get_sequence(addr *start, addr *end) {
  *start = sequence_start_;
  *end = sequence_end_;
}

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
    safe_error(error) = "Fiver does not support more than one transaction";
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
  if (pickle())
    return true;
  else
    return false;
};

void Fiver::commit_() {
  if (!built_) {
    storage_estimate_ =
        text_->size() + sizeof(Annotation) * annotations_->size();
    appender_->commit();
    annotator_->commit();
    annotations_->clear();
    idx_ = FiverIdx::make(index_);
    assert(idx_ != nullptr);
    txt_ = FiverTxt::make(featurizer_, tokenizer_, idx_, text_);
    assert(txt_ != nullptr);
    built_ = true;
    if (working_ != nullptr) {
      std::string ready_name = working()->make_name(name() + "." + recipe());
      name_ = "fiver";
      std::string final_name = working()->make_name(name() + "." + recipe());
      assert(link(ready_name.c_str(), final_name.c_str()) == 0);
      std::remove(ready_name.c_str());
    } else {
      name_ = "fiver";
    }
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
    discard();
    name_ = "remove";
  }
};

bool Fiver::discard(std::string *error) {
  if (working() != nullptr) {
    std::string jarname = working()->make_name(name() + "." + recipe());
    std::remove(jarname.c_str());
  }
  return true;
}

bool Fiver::pickle(std::string *error) {
  if (working() != nullptr) {
    std::string tempname = working()->make_temp();
    if (!pickle(tempname, error))
      return false;
    std::string jarname = working()->make_name(name() + "." + recipe());
    if (link(tempname.c_str(), jarname.c_str()) != 0) {
      safe_error(error) = "Fiver can't link pickle jar";
      return false;
    }
    std::remove(tempname.c_str());
  }
  return true;
}

bool Fiver::pickle(const std::string &filename, std::string *error) {
  if (idx_ == nullptr || txt_ == nullptr) {
    safe_error(error) = "Fiver must have Idx and Txt before pickling";
    return false;
  }
  if (text_compressor_->destructive()) {
    safe_error(error) = "Fiver's text compressor can't be destructive";
    return false;
  }
  std::fstream jar;
  jar.open(filename, std::ios::binary | std::ios::out);
  if (jar.fail()) {
    safe_error(error) = "Fiver can't create pickle jar: " + filename;
    return false;
  }
  jar.write(reinterpret_cast<char *>(&sequence_start_),
            sizeof(sequence_start_));
  if (jar.fail()) {
    safe_error(error) = "Fiver failed to pickle into: " + filename;
    jar.close();
    return false;
  }
  jar.write(reinterpret_cast<char *>(&sequence_end_), sizeof(sequence_end_));
  if (jar.fail()) {
    safe_error(error) = "Fiver failed to pickle into: " + filename;
    jar.close();
    return false;
  }
  // DELETE THIS LINE SOME DAY: addr n = text_->length() + 1;
  addr n = text_->length();
  jar.write(reinterpret_cast<char *>(&n), sizeof(n));
  addr m = n + text_compressor_->extra(n);
  std::unique_ptr<char[]> buffer = std::unique_ptr<char[]>(new char[m]);
  m = text_compressor_->crush((char *)(text_->c_str()), n, buffer.get(), m);
  jar.write(reinterpret_cast<char *>(&m), sizeof(m));
  if (jar.fail()) {
    safe_error(error) = "Fiver failed to pickle into: " + filename;
    jar.close();
    return false;
  }
  jar.write(buffer.get(), m);
  if (jar.fail()) {
    safe_error(error) = "Fiver failed to pickle into: " + filename;
    return false;
    jar.close();
  }
  for (auto &posting : *index_)
    posting.second->write(&jar);
  jar.close();
  return true;
}

std::shared_ptr<Fiver>
Fiver::unpickle(const std::string &filename, std::shared_ptr<Working> working,
                std::shared_ptr<Featurizer> featurizer,
                std::shared_ptr<Tokenizer> tokenizer, std::string *error,
                std::shared_ptr<Compressor> posting_compressor,
                std::shared_ptr<Compressor> fvalue_compressor,
                std::shared_ptr<Compressor> text_compressor) {
  if (featurizer == nullptr) {
    safe_error(error) = "Fiver needs a featurizer (got nullptr)";
    return nullptr;
  }
  if (tokenizer == nullptr) {
    safe_error(error) = "Fiver needs a tokenizer (got nullptr)";
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
    if (working != nullptr) {
      jarname = filename;
      jar.open(jarname, std::ios::binary | std::ios::in);
    }
    if (jar.fail()) {
      safe_error(error) = "Fiver can't open pickle jar: " + jarname;
      return nullptr;
    }
  }
  std::shared_ptr<Fiver> fiver = std::shared_ptr<Fiver>(
      new Fiver(working, featurizer, tokenizer, nullptr, nullptr));
  if (fiver == nullptr)
    return nullptr;
  fiver->built_ = true;
  fiver->where_ = 0;
  fiver->name_ = "fiver";
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
  if (jar.fail()) {
    safe_error(error) = "Fiver can't unpickle: " + jarname;
    jar.close();
    return nullptr;
  }
  jar.read(reinterpret_cast<char *>(&fiver->sequence_end_),
           sizeof(fiver->sequence_end_));
  if (jar.fail()) {
    safe_error(error) = "Fiver can't unpickle: " + jarname;
    jar.close();
    return nullptr;
  }
  addr n;
  jar.read(reinterpret_cast<char *>(&n), sizeof(n));
  if (jar.fail()) {
    safe_error(error) = "Fiver can't unpickle: " + jarname;
    jar.close();
    return nullptr;
  }
  addr nx = n + fiver->text_compressor_->extra(n);
  std::unique_ptr<char[]> uncompressed = std::unique_ptr<char[]>(new char[nx]);
  addr m;
  jar.read(reinterpret_cast<char *>(&m), sizeof(m));
  if (jar.fail()) {
    safe_error(error) = "Fiver can't unpickle: " + jarname;
    jar.close();
    return nullptr;
  }
  std::unique_ptr<char[]> compressed = std::unique_ptr<char[]>(new char[m]);
  jar.read(reinterpret_cast<char *>(compressed.get()), m);
  fiver->text_compressor_->tang(compressed.get(), m, uncompressed.get(), nx);
  fiver->text_ = std::make_shared<std::string>(uncompressed.get(), n);
  std::shared_ptr<SimplePostingFactory> factory = SimplePostingFactory::make(
      fiver->posting_compressor_, fiver->fvalue_compressor_);
  std::make_shared<std::map<addr, std::shared_ptr<SimplePosting>>>();
  fiver->index_ =
      std::make_shared<std::map<addr, std::shared_ptr<SimplePosting>>>();
  addr total_annotations = 0;
  for (;;) {
    std::shared_ptr<SimplePosting> posting = factory->posting_from_file(&jar);
    if (posting == nullptr)
      break;
    total_annotations += sizeof(Annotation) * posting->size();
    (*fiver->index_)[posting->feature()] = posting;
  }
  jar.close();
  fiver->storage_estimate_ =
      fiver->text_->size() + sizeof(Annotation) * total_annotations;
  fiver->idx_ = FiverIdx::make(fiver->index_, error);
  if (fiver->idx_ == nullptr)
    return nullptr;
  fiver->txt_ =
      FiverTxt::make(featurizer, tokenizer, fiver->idx_, fiver->text_, error);
  if (fiver->txt_ == nullptr)
    return nullptr;
  return fiver;
}

namespace {

struct HazelBlob {
  std::string name;
  addr offset;
  addr length;
};

struct HazelPostingEntry {
  addr feature;
  addr end;
  addr count;
};

struct HazelTextEntry {
  addr raw_end;
  addr compressed_end;
};

template <typename T> void hazel_write_pod(std::ostream *out, const T &value) {
  out->write(reinterpret_cast<const char *>(&value), sizeof(value));
}

void hazel_write_string(std::ostream *out, const std::string &value) {
  addr length = value.size();
  hazel_write_pod(out, length);
  out->write(value.data(), value.size());
}

std::string hazel_package(const std::string &name, const std::string &recipe) {
  std::map<std::string, std::string> parameters;
  parameters["name"] = name;
  parameters["recipe"] = recipe;
  return freeze(parameters);
}

std::string hazel_dna(std::shared_ptr<Featurizer> featurizer,
                      std::shared_ptr<Tokenizer> tokenizer,
                      std::shared_ptr<Compressor> posting_compressor,
                      std::shared_ptr<Compressor> fvalue_compressor,
                      std::shared_ptr<Compressor> text_compressor,
                      addr sequence_start, addr sequence_end,
                      addr text_chunk_size) {
  std::map<std::string, std::string> idx_recipe;
  idx_recipe["posting_compressor"] = posting_compressor->name();
  idx_recipe["posting_compressor_recipe"] = posting_compressor->recipe();
  idx_recipe["fvalue_compressor"] = fvalue_compressor->name();
  idx_recipe["fvalue_compressor_recipe"] = fvalue_compressor->recipe();
  std::map<std::string, std::string> idx;
  idx["name"] = "hazel";
  idx["recipe"] = freeze(idx_recipe);

  std::map<std::string, std::string> txt_recipe;
  txt_recipe["compressor"] = text_compressor->name();
  txt_recipe["compressor_recipe"] = text_compressor->recipe();
  txt_recipe["chunk_size"] = std::to_string(text_chunk_size);
  std::map<std::string, std::string> txt;
  txt["name"] = "hazel";
  txt["recipe"] = freeze(txt_recipe);

  std::map<std::string, std::string> metadata;
  metadata["sequence_start"] = std::to_string(sequence_start);
  metadata["sequence_end"] = std::to_string(sequence_end);

  std::map<std::string, std::string> dna;
  dna["warren"] = "hazel";
  dna["featurizer"] = hazel_package(featurizer->name(), featurizer->recipe());
  dna["tokenizer"] = hazel_package(tokenizer->name(), tokenizer->recipe());
  dna["idx"] = freeze(idx);
  dna["txt"] = freeze(txt);
  dna["hazel"] = freeze(metadata);
  return freeze(dna);
}

addr hazel_tellp(std::fstream *out) { return out->tellp(); }

bool hazel_write_idx_blob(
    std::fstream *out,
    const std::map<addr, std::shared_ptr<SimplePosting>> &index,
    addr *blob_start, addr *blob_length, std::string *error) {
  *blob_start = hazel_tellp(out);
  const std::string magic = "COTTONTAIL_HAZEL_IDX_V1\n";
  out->write(magic.data(), magic.size());
  addr directory_offset = 0;
  addr directory_length = 0;
  addr directory_count = 0;
  hazel_write_pod(out, directory_offset);
  hazel_write_pod(out, directory_length);
  hazel_write_pod(out, directory_count);

  std::vector<HazelPostingEntry> directory;
  for (auto &posting : index) {
    HazelPostingEntry entry;
    entry.feature = posting.first;
    entry.end = hazel_tellp(out) - *blob_start;
    entry.count = posting.second->size();
    addr p, q;
    fval v;
    if (entry.count == 1 && posting.second->get(0, &p, &q, &v) && p == q &&
        v == 0.0) {
      entry.count = p;
    } else {
      posting.second->write(out);
      entry.end = hazel_tellp(out) - *blob_start;
      if (out->fail()) {
        safe_error(error) = "Fiver failed to write Hazel idx postings";
        return false;
      }
    }
    directory.push_back(entry);
  }
  directory_offset = hazel_tellp(out) - *blob_start;
  directory_count = directory.size();
  for (auto &entry : directory) {
    hazel_write_pod(out, entry.feature);
    hazel_write_pod(out, entry.end);
    hazel_write_pod(out, entry.count);
  }
  directory_length = hazel_tellp(out) - *blob_start - directory_offset;
  *blob_length = hazel_tellp(out) - *blob_start;
  out->seekp(*blob_start + magic.size());
  hazel_write_pod(out, directory_offset);
  hazel_write_pod(out, directory_length);
  hazel_write_pod(out, directory_count);
  out->seekp(*blob_start + *blob_length);
  if (out->fail()) {
    safe_error(error) = "Fiver failed to write Hazel idx directory";
    return false;
  }
  return true;
}

bool hazel_crush(std::shared_ptr<Compressor> compressor,
                 const char *raw, addr raw_length, std::string *compressed,
                 std::string *error) {
  addr available = raw_length + compressor->extra(raw_length);
  std::unique_ptr<char[]> in;
  char *source = const_cast<char *>(raw);
  if (compressor->destructive()) {
    in = std::unique_ptr<char[]>(new char[raw_length + 1]);
    memcpy(in.get(), raw, raw_length);
    source = in.get();
  }
  std::unique_ptr<char[]> out = std::unique_ptr<char[]>(new char[available + 1]);
  addr n = compressor->crush(source, raw_length, out.get(), available);
  if (n > available) {
    safe_error(error) = "Hazel compressor overflow";
    return false;
  }
  compressed->assign(out.get(), n);
  return true;
}

bool hazel_write_text_chunk(std::fstream *out, addr blob_start,
                            const std::string &text, addr raw_start,
                            addr raw_length,
                            std::shared_ptr<Compressor> text_compressor,
                            std::vector<HazelTextEntry> *directory,
                            std::string *error) {
  if (raw_length <= 0)
    return true;
  std::string compressed;
  if (!hazel_crush(text_compressor, text.data() + raw_start, raw_length,
                   &compressed, error))
    return false;
  HazelTextEntry entry;
  entry.raw_end = raw_start + raw_length;
  out->write(compressed.data(), compressed.size());
  entry.compressed_end = hazel_tellp(out) - blob_start;
  directory->push_back(entry);
  if (out->fail()) {
    safe_error(error) = "Fiver failed to write Hazel txt chunk";
    return false;
  }
  return true;
}

bool hazel_write_txt_blob(std::fstream *out, std::shared_ptr<Idx> idx,
                          std::shared_ptr<Featurizer> featurizer,
                          const std::string &text,
                          std::shared_ptr<Compressor> text_compressor,
                          addr target_chunk_size,
                          addr *blob_start, addr *blob_length,
                          std::string *error) {
  *blob_start = hazel_tellp(out);
  const std::string magic = "COTTONTAIL_HAZEL_TXT_V1\n";
  out->write(magic.data(), magic.size());
  addr directory_offset = 0;
  addr directory_length = 0;
  addr directory_count = 0;
  addr raw_text_length = text.size();
  hazel_write_pod(out, directory_offset);
  hazel_write_pod(out, directory_length);
  hazel_write_pod(out, directory_count);
  hazel_write_pod(out, raw_text_length);
  hazel_write_pod(out, target_chunk_size);

  std::vector<HazelTextEntry> directory;
  std::unique_ptr<Hopper> hopper =
      idx->hopper(featurizer->featurize(text_chunk_tag));
  addr p, q;
  fval v;
  bool have_chunk = false;
  addr group_start = 0;
  for (hopper->tau(minfinity + 1, &p, &q, &v); p < maxfinity;
       hopper->tau(p + 1, &p, &q, &v)) {
    addr raw_start = fval2addr(v);
    if (raw_start < 0 || raw_start > (addr)text.size()) {
      safe_error(error) = "Fiver has invalid text chunk offset";
      return false;
    }
    if (!have_chunk) {
      group_start = raw_start;
      have_chunk = true;
    } else if (raw_start - group_start >= target_chunk_size) {
      if (!hazel_write_text_chunk(out, *blob_start, text, group_start,
                                  raw_start - group_start, text_compressor,
                                  &directory, error))
        return false;
      group_start = raw_start;
    }
  }
  if (have_chunk) {
    if (!hazel_write_text_chunk(out, *blob_start, text, group_start,
                                (addr)text.size() - group_start,
                                text_compressor, &directory, error))
      return false;
  } else if (text.size() > 0) {
    if (!hazel_write_text_chunk(out, *blob_start, text, 0, text.size(),
                                text_compressor, &directory, error))
      return false;
  }
  directory_offset = hazel_tellp(out) - *blob_start;
  directory_count = directory.size();
  for (auto &entry : directory) {
    hazel_write_pod(out, entry.raw_end);
    hazel_write_pod(out, entry.compressed_end);
  }
  directory_length = hazel_tellp(out) - *blob_start - directory_offset;
  *blob_length = hazel_tellp(out) - *blob_start;
  out->seekp(*blob_start + magic.size());
  hazel_write_pod(out, directory_offset);
  hazel_write_pod(out, directory_length);
  hazel_write_pod(out, directory_count);
  hazel_write_pod(out, raw_text_length);
  hazel_write_pod(out, target_chunk_size);
  out->seekp(*blob_start + *blob_length);
  if (out->fail()) {
    safe_error(error) = "Fiver failed to write Hazel txt directory";
    return false;
  }
  return true;
}

std::string hazel_blob_dictionary(const std::vector<HazelBlob> &blobs) {
  std::ostringstream out(std::ios::out | std::ios::binary);
  const std::string magic = "COTTONTAIL_HAZEL_BLOBS_V1\n";
  out.write(magic.data(), magic.size());
  addr n = blobs.size();
  hazel_write_pod(&out, n);
  for (auto &blob : blobs) {
    hazel_write_string(&out, blob.name);
    hazel_write_pod(&out, blob.offset);
    hazel_write_pod(&out, blob.length);
  }
  return out.str();
}

std::string hazel_default_name(addr sequence_start, addr sequence_end) {
  return "hazel." + seq2str(sequence_start) + "." + seq2str(sequence_end);
}

} // namespace

bool Fiver::hazel(std::string *error, bool discard, addr text_chunk_size) {
  if (working() == nullptr) {
    safe_error(error) = "Fiver needs a working directory for default Hazel name";
    return false;
  }
  if (text_chunk_size <= 0) {
    safe_error(error) = "Hazel text chunk size must be positive";
    return false;
  }
  std::string tempname = working()->make_temp("hazel");
  if (!hazel(tempname, error, false, text_chunk_size)) {
    std::remove(tempname.c_str());
    return false;
  }
  std::string hazelname =
      working()->make_name(hazel_default_name(sequence_start_, sequence_end_));
  if (link(tempname.c_str(), hazelname.c_str()) != 0) {
    safe_error(error) = "Fiver can't link Hazel shard: " + hazelname;
    std::remove(tempname.c_str());
    return false;
  }
  std::remove(tempname.c_str());
  if (discard)
    return Fiver::discard(error);
  return true;
}

bool Fiver::hazel(const std::string &filename, std::string *error,
                  bool discard, addr text_chunk_size) {
  if (idx_ == nullptr || txt_ == nullptr || index_ == nullptr ||
      text_ == nullptr) {
    safe_error(error) = "Fiver must have Idx and Txt before writing Hazel";
    return false;
  }
  if (text_chunk_size <= 0) {
    safe_error(error) = "Hazel text chunk size must be positive";
    return false;
  }
  std::string dna =
      hazel_dna(featurizer_, tokenizer_, posting_compressor_,
                fvalue_compressor_, text_compressor_, sequence_start_,
                sequence_end_, text_chunk_size);

  std::vector<HazelBlob> blobs = {{"idx", 0, 0}, {"txt", 0, 0}};
  const std::string file_header = "#COTTONTAIL\n";
  std::string dictionary = hazel_blob_dictionary(blobs);

  std::fstream out;
  out.open(filename, std::ios::binary | std::ios::out);
  if (out.fail()) {
    safe_error(error) = "Fiver can't create Hazel shard: " + filename;
    return false;
  }
  out.write(file_header.data(), file_header.size());
  out.write(dna.data(), dna.size());
  out.put('\n');
  out.put('\n');
  addr dictionary_offset = hazel_tellp(&out);
  out.write(dictionary.data(), dictionary.size());
  if (!hazel_write_idx_blob(&out, *index_, &blobs[0].offset, &blobs[0].length,
                            error)) {
    out.close();
    return false;
  }
  if (!hazel_write_txt_blob(&out, idx(), featurizer_, *text_, text_compressor_,
                            text_chunk_size, &blobs[1].offset,
                            &blobs[1].length, error)) {
    out.close();
    return false;
  }
  addr end = hazel_tellp(&out);
  dictionary = hazel_blob_dictionary(blobs);
  out.seekp(dictionary_offset);
  out.write(dictionary.data(), dictionary.size());
  out.seekp(end);
  if (out.fail()) {
    safe_error(error) = "Fiver failed to write Hazel shard: " + filename;
    out.close();
    return false;
  }
  out.close();
  if (discard)
    return Fiver::discard(error);
  return true;
}

} // namespace cottontail
