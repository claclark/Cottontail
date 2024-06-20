#include "src/bigwig.h"

#include <algorithm>
#include <map>
#include <memory>
#include <regex>
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
#include "src/vector_hopper.h"
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
    assert(annotator != nullptr);
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
  bool erase_(addr p, addr q, std::string *error) {
    return fiver_->annotator()->erase(p, q, error);
  }
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
    std::shared_ptr<BigwigAppender> appender =
        std::shared_ptr<BigwigAppender>(new BigwigAppender());
    assert(appender != nullptr);
    appender->fiver_ = fiver;
    return appender;
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
  make(const std::vector<std::shared_ptr<Fiver>> &warrens) {
    std::shared_ptr<BigwigIdx> idx =
        std::shared_ptr<BigwigIdx>(new BigwigIdx());
    assert(idx != nullptr);
    idx->warrens_ = warrens;
    idx->erasing_ = false;
    for (auto &&warren : warrens)
      if (warren->idx()->count(null_feature)) {
        idx->erasing_ = true;
        break;
      }
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
    if (erasing_ && feature != null_feature) {
      return std::make_unique<cottontail::gcl::NotContainedIn>(
          std::move(Fiver::merge(warrens_, feature)),
          std::move(Fiver::merge(warrens_, null_feature)));
    } else {
      return Fiver::merge(warrens_, feature);
    }
  };
  addr count_(addr feature) final {
    addr n = 0;
    for (auto &warren : warrens_)
      if (warren != nullptr)
        n += warren->idx()->count(feature);
    return n;
  }
  addr vocab_() final {
    addr n = 0;
    for (auto &warren : warrens_)
      if (warren != nullptr)
        n += warren->idx()->vocab();
    return n;
  }
  std::vector<std::shared_ptr<Fiver>> warrens_;
  bool erasing_;
};

class BigwigTxt final : public Txt {
public:
  static std::shared_ptr<Txt>
  make(const std::vector<std::shared_ptr<Fiver>> &warrens) {
    std::shared_ptr<BigwigTxt> txt =
        std::shared_ptr<BigwigTxt>(new BigwigTxt());
    assert(txt != nullptr);
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
  std::string name_() final { return "bigwig"; };
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
  bool range_(addr *p, addr *q) {
    if (warrens_.size() == 0) {
      *p = *q = maxfinity;
      return false;
    }
    addr q0;
    size_t i;
    for (i = 0; i < warrens_.size(); i++)
      if (warrens_[i]->txt()->range(p, &q0))
        break;
    if (i == warrens_.size()) {
      *p = *q = maxfinity;
      return false;
    }
    addr p1;
    size_t j;
    for (j = warrens_.size() - 1; j > i; --j)
      if (warrens_[j]->txt()->range(&p1, q))
        break;
    if (j == i)
      *q = q0;
    return true;
  };
  std::vector<std::shared_ptr<Fiver>> warrens_;
};

namespace {
bool fiver_files(std::shared_ptr<Working> working,
                 std::vector<std::string> *name, std::string *error) {
  if (working == nullptr) {
    if (name != nullptr)
      name->clear();
    return true;
  }
  std::vector<std::string> fivers = working->ls("fiver");
  struct FiverFile {
    FiverFile(addr start, cottontail::addr end, const std::string &name)
        : start(start), end(end), name(name){};
    addr start;
    addr end;
    std::string name;
  };
  std::vector<FiverFile> found;
  for (auto &fiver : fivers) {
    std::string suffix = fiver.substr(fiver.find(".") + 1);
    try {
      addr start = std::stol(suffix.substr(0, suffix.find(".")));
      addr end = std::stol(suffix.substr(suffix.find(".") + 1));
      if (start < 0 || end < 0 || start > end) {
        safe_set(error) = "fiver filename range error: " + fiver;
        return false;
      }
      found.emplace_back(start, end, fiver);
    } catch (const std::invalid_argument &e) {
      safe_set(error) = "fiver filename format error: " + fiver;
      return false;
    }
  }
  std::sort(found.begin(), found.end(),
            [](const auto &a, const auto &b) -> bool {
              return a.start < b.start || (a.start == b.start && a.end > b.end);
            });
  std::vector<FiverFile> living;
  std::vector<FiverFile> dead;
  for (auto &fiver : found)
    if (living.empty() || living.back().end < fiver.start) {
      living.emplace_back(fiver);
    } else if (living.back().end >= fiver.end) {
      dead.emplace_back(fiver);
    } else {
      safe_set(error) = "fiver filename sequence error: " + fiver.name;
      return false;
    }
  // can't see a situation when it isn't okay to remove
  // uncommited transactions ("kittens")
  std::vector<std::string> kittens = working->ls("kitten");
  for (auto &kitten : kittens)
    std::remove(working->make_name(kitten).c_str());
  std::vector<std::string> temps = working->ls("temp");
  for (auto &temp : temps)
    std::remove(working->make_name(temp).c_str());
  for (auto &fiver : dead)
    std::remove(working->make_name(fiver.name).c_str());
  if (name != nullptr) {
    name->clear();
    for (auto &fiver : living)
      name->push_back(working->make_name(fiver.name));
  }
  return true;
}

const std::string default_dna = "["
                                "  featurizer:["
                                "    name:\"hashing\","
                                "    recipe:\"\","
                                "  ],"
                                "  idx:["
                                "    name:\"bigwig\","
                                "    recipe:["
                                "      fvalue_compressor:\"null\","
                                "      fvalue_compressor_recipe:\"\","
                                "      posting_compressor:\"null\","
                                "      posting_compressor_recipe:\"\","
                                "    ],"
                                "  ],"
                                "  parameters:[],"
                                "  tokenizer:["
                                "    name:\"utf8\","
                                "    recipe:\"\","
                                "  ],"
                                "  txt:["
                                "    name:\"bigwig\","
                                "    recipe:["
                                "      compressor:\"null\","
                                "      compressor_recipe:\"\","
                                "      json:\"no\","
                                "    ],"
                                "  ],"
                                "  warren:\"bigwig\","
                                "]";

} // namespace

std::shared_ptr<Bigwig> Bigwig::make(const std::string &burrow,
                                     const std::string &recipe,
                                     std::string *error) {
  std::string the_burrow = burrow;
  if (the_burrow == "")
    the_burrow = DEFAULT_BURROW;
  std::shared_ptr<Working> working = Working::mkdir(the_burrow, error);
  if (working == nullptr)
    return nullptr;
  std::string dna;
  if (read_dna(working, &dna)) {
    safe_set(error) = "Burrow already has cottontail dna";
    return nullptr;
  }
  dna = default_dna;
  std::regex whitespace("\\s+");
  std::vector<std::string> options{
      std::sregex_token_iterator(recipe.begin(), recipe.end(), whitespace, -1),
      {}};
  for (auto &option : options)
    if (!interpret_option(&dna, option, error))
      return nullptr;
  if (!write_dna(working, dna, error))
    return nullptr;
  return make(the_burrow, error);
}

std::shared_ptr<Bigwig> Bigwig::make(const std::string &burrow,
                                     std::string *error) {
  std::string the_burrow = burrow;
  if (the_burrow == "")
    the_burrow = DEFAULT_BURROW;
  std::shared_ptr<Working> working = Working::make(the_burrow, error);
  if (working == nullptr)
    return nullptr;
  std::string dna;
  if (!read_dna(working, &dna, error))
    return nullptr;
  std::map<std::string, std::string> parameters;
  if (!cook(dna, &parameters, error))
    return nullptr;
  std::map<std::string, std::string> featurizer_parameters;
  if (!cook(parameters["featurizer"], &featurizer_parameters, error))
    return nullptr;
  std::shared_ptr<Featurizer> featurizer =
      Featurizer::make(featurizer_parameters["name"],
                       featurizer_parameters["recipe"], error, working);
  if (featurizer == nullptr)
    return nullptr;
  std::map<std::string, std::string> tokenizer_parameters;
  if (!cook(parameters["tokenizer"], &tokenizer_parameters, error))
    return nullptr;
  std::shared_ptr<Tokenizer> tokenizer = Tokenizer::make(
      tokenizer_parameters["name"], tokenizer_parameters["recipe"], error);
  if (tokenizer == nullptr)
    return nullptr;
  std::map<std::string, std::string> idx_parameters;
  if (!cook(parameters["idx"], &idx_parameters, error))
    return nullptr;
  std::map<std::string, std::string> idx_recipe_parameters;
  if (!cook(idx_parameters["recipe"], &idx_recipe_parameters, error))
    return nullptr;
  std::shared_ptr<Compressor> posting_compressor = Compressor::make(
      idx_recipe_parameters["posting_compressor"],
      idx_recipe_parameters["posting_compressor_recipe"], error);
  if (posting_compressor == nullptr)
    return nullptr;
  std::shared_ptr<Compressor> fvalue_compressor = Compressor::make(
      idx_recipe_parameters["fvalue_compressor"],
      idx_recipe_parameters["fvalue_compressor_recipe"], error);
  if (fvalue_compressor == nullptr)
    return nullptr;
  std::map<std::string, std::string> txt_parameters;
  if (!cook(parameters["txt"], &txt_parameters, error))
    return nullptr;
  std::map<std::string, std::string> txt_recipe_parameters;
  if (!cook(txt_parameters["recipe"], &txt_recipe_parameters, error))
    return nullptr;
  std::shared_ptr<Compressor> text_compressor =
      Compressor::make(txt_recipe_parameters["compressor"],
                       txt_recipe_parameters["compressor_recipe"], error);
  if (text_compressor == nullptr)
    return nullptr;
  std::map<std::string, std::string> extra_parameters;
  std::string container_query;
  std::shared_ptr<Stemmer> stemmer;
  if (parameters.find("parameters") != parameters.end()) {
    if (!cook(parameters["parameters"], &extra_parameters, error))
      return nullptr;
    auto container_element = extra_parameters.find("container");
    if (container_element != extra_parameters.end())
      container_query = container_element->second;
    std::string stemmer_name, stemmer_recipe;
    auto stemmer_element = extra_parameters.find("stemmer");
    if (stemmer_element != extra_parameters.end()) {
      std::string stemmer_name, stemmer_recipe;
      stemmer_name = stemmer_element->second;
    }
    if (stemmer_name != "") {
      stemmer = Stemmer::make(stemmer_name, stemmer_recipe, error);
      if (stemmer == nullptr)
        return nullptr;
    }
  }
  std::shared_ptr<Fluffle> fluffle = Fluffle::make();
  (*fluffle->parameters) = extra_parameters;
  std::vector<std::string> fivernames;
  if (!fiver_files(working, &fivernames, error))
    return nullptr;
  std::vector<std::shared_ptr<Fiver>> fivers;
  for (auto &fivername : fivernames) {
    std::shared_ptr<Fiver> fiver =
        Fiver::unpickle(fivername, working, featurizer, tokenizer, error,
                        posting_compressor, fvalue_compressor, text_compressor);
    if (fiver == nullptr)
      return nullptr;
    fiver->start();
    fivers.push_back(fiver);
    fluffle->warrens.push_back(fiver);
  }
  addr address = 0;
  addr sequence = 0;
  if (fivers.size() > 0) {
    addr p, q;
    for (size_t i = fivers.size(); i > 0; --i) {
      if (fivers[i - 1]->txt()->range(&p, &q)) {
        address = q + 1;
        break;
      }
    }
    fivers.back()->get_sequence(&p, &q);
    sequence = q + 1;
  }
  fluffle->address = address;
  fluffle->sequence = sequence;
  std::shared_ptr<Bigwig> bigwig = std::shared_ptr<Bigwig>(
      new Bigwig(working, featurizer, tokenizer, nullptr, nullptr));
  assert(bigwig != nullptr);
  bigwig->fiver_ = nullptr;
  bigwig->fluffle_ = fluffle;
  bigwig->posting_compressor_ = posting_compressor;
  bigwig->fvalue_compressor_ = fvalue_compressor;
  bigwig->text_compressor_ = text_compressor;
  bigwig->txt_recipe_ = txt_parameters["recipe"];
  if (stemmer != nullptr)
    bigwig->set_stemmer(stemmer);
  if (container_query != "")
    bigwig->set_default_container(container_query);
  std::string do_merge;
  assert(bigwig->get_parameter("merge", &do_merge));
  fluffle->merge = (do_merge == "" || okay(do_merge));
  bigwig->try_merge();
  return bigwig;
}

std::shared_ptr<Bigwig> Bigwig::make(
    std::shared_ptr<Working> working, std::shared_ptr<Featurizer> featurizer,
    std::shared_ptr<Tokenizer> tokenizer, std::shared_ptr<Fluffle> fluffle,
    std::string *error, std::shared_ptr<Compressor> posting_compressor,
    std::shared_ptr<Compressor> fvalue_compressor,
    std::shared_ptr<Compressor> text_compressor) {
  if (working != nullptr) {
    std::string dna;
    if (read_dna(working, &dna, error)) {
      safe_set(error) = "Burrow already has cottontail dna";
      return nullptr;
    }
  }
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
  assert(bigwig != nullptr);
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
  if (working != nullptr && !write_dna(working, bigwig->recipe(), error))
    return nullptr;
  return bigwig;
}

void Bigwig::merge(bool on) {
  set_parameter("merge", okay(on));
  if (on) {
    fluffle_->merge = true;
    try_merge();
  } else {
    fluffle_->merge = false;
  }
}

std::shared_ptr<Warren> Bigwig::clone_(std::string *error) {
  std::shared_ptr<Bigwig> bigwig = std::shared_ptr<Bigwig>(
      new Bigwig(working_, featurizer_, tokenizer_, nullptr, nullptr));
  assert(bigwig != nullptr);
  bigwig->fluffle_ = fluffle_;
  bigwig->posting_compressor_ = posting_compressor_;
  bigwig->fvalue_compressor_ = fvalue_compressor_;
  bigwig->text_compressor_ = text_compressor_;
  bigwig->default_container_ = default_container_;
  if (stemmer_ != nullptr) {
    std::shared_ptr<cottontail::Stemmer> the_stemmer =
        cottontail::Stemmer::make(stemmer_->name(), stemmer_->recipe(), error);
    if (the_stemmer == nullptr)
      return nullptr;
    bigwig->stemmer_ = the_stemmer;
  }
  return bigwig;
}

void Bigwig::start_() {
  fluffle_->lock.lock();
  for (auto &warren : fluffle_->warrens)
    if (warren != nullptr && warren->name() == "fiver")
      warrens_.push_back(std::static_pointer_cast<Fiver>(warren));
  fluffle_->lock.unlock();
  idx_ = BigwigIdx::make(warrens_);
  assert(idx_ != nullptr);
  txt_ = Txt::wrap(txt_recipe_, BigwigTxt::make(warrens_));
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
  if (working_ != nullptr &&
      !set_parameter_in_dna(working_, key, value, error)) {
    fluffle_->lock.unlock();
    return false;
  }
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
  fiver_->set_sequence(fluffle_->sequence);
  fluffle_->sequence++;
  fluffle_->warrens.push_back(fiver_);
  fluffle_->lock.unlock();
  return fiver_->ready();
}

namespace {
void merge_worker(std::shared_ptr<Fluffle> fluffle) {
  for (;;) {
    fluffle->lock.lock();
    if (fluffle->warrens.size() < 2) {
      assert(fluffle->workers > 0);
      --fluffle->workers;
      fluffle->lock.unlock();
      return;
    }
    bool cleanup = false;
    for (auto &warren : fluffle->warrens)
      if (warren->name() == "remove") {
        cleanup = true;
        break;
      }
    if (cleanup) {
      std::vector<std::shared_ptr<Warren>> warrens;
      for (auto &warren : fluffle->warrens)
        if (warren->name() != "remove")
          warrens.push_back(warren);
      fluffle->warrens = warrens;
    }
    if (fluffle->warrens.size() < 2) {
      assert(fluffle->workers > 0);
      --fluffle->workers;
      fluffle->lock.unlock();
      return;
    }
    std::vector<addr> info;
    for (size_t i = 0; i < fluffle->warrens.size(); i++) {
      if (fluffle->warrens[i] != nullptr &&
          fluffle->warrens[i]->name() == "fiver" &&
          fluffle->merging.find(fluffle->warrens[i]) ==
              fluffle->merging.end()) {
        std::shared_ptr<Fiver> fiver =
            std::static_pointer_cast<Fiver>(fluffle->warrens[i]);
        addr a, b;
        fiver->get_sequence(&a, &b);
        info.push_back(b - a);
      } else {
        info.push_back(-1);
      }
    }
    size_t start = 0, end = 0;
    bool have_range = false;
    size_t current;
    bool in_range = false;
    for (size_t i = 0; i < info.size(); i++)
      if (in_range) {
        if (info[i] != 0) {
          in_range = false;
          if (have_range) {
            addr len0 = (end - start) + 1;
            addr len1 = i - current;
            if (len1 > len0) {
              start = current;
              end = i - 1;
            }
          } else if ((i - current) >= 2) {
            have_range = true;
            start = current;
            end = i - 1;
          }
        }
      } else if (info[i] == 0) {
        in_range = true;
        current = i;
      }
    if (in_range) {
      if (have_range) {
        addr len0 = (end - start) + 1;
        addr len1 = info.size() - current;
        if (len1 > len0) {
          start = current;
          end = info.size() - 1;
        }
      } else if ((info.size() - current) >= 2) {
        have_range = true;
        start = current;
        end = info.size() - 1;
      }
    }
    if (!have_range) {
      addr gap = maxfinity;
      for (size_t i = info.size() - 1; i > 0; --i)
        if (info[i] >= 0 && info[i - 1] >= 0 &&
            std::abs(info[i] - info[i - 1]) < gap) {
          have_range = true;
          gap = std::abs(info[i] - info[i - 1]);
          start = i - 1;
          end = i;
        }
    }
    if (!have_range) {
      assert(fluffle->workers > 0);
      --fluffle->workers;
      fluffle->lock.unlock();
      return;
    }
    std::vector<std::shared_ptr<Fiver>> fivers;
    for (size_t i = start; i <= end; i++) {
      fivers.push_back(std::static_pointer_cast<Fiver>(fluffle->warrens[i]));
      fluffle->merging.insert(fluffle->warrens[i]);
    }
    std::shared_ptr<Warren> start_warren = fluffle->warrens[start];
    std::shared_ptr<Warren> end_warren = fluffle->warrens[end];
    if (fluffle->workers < fluffle->max_workers) {
      fluffle->workers++;
      std::thread t(merge_worker, fluffle);
      t.detach();
    }
    fluffle->lock.unlock();
    std::shared_ptr<Fiver> merged = Fiver::merge(fivers);
    merged->pickle();
    for (auto &fiver : fivers)
      fiver->discard();
    fluffle->lock.lock();
    std::vector<std::shared_ptr<Warren>> warrens;
    size_t i;
    for (i = 0;
         i < fluffle->warrens.size() && fluffle->warrens[i] != start_warren;
         i++)
      warrens.push_back(fluffle->warrens[i]);
    assert(i < fluffle->warrens.size());
    fluffle->merging.erase(fluffle->warrens[i]);
    if (merged != nullptr) {
      warrens.push_back(merged);
      merged->start();
    }
    for (i++; i < fluffle->warrens.size() && fluffle->warrens[i] != end_warren;
         i++)
      fluffle->merging.erase(fluffle->warrens[i]);
    ;
    assert(i < fluffle->warrens.size());
    fluffle->merging.erase(fluffle->warrens[i]);
    for (i++; i < fluffle->warrens.size(); i++)
      warrens.push_back(fluffle->warrens[i]);
    fluffle->warrens = warrens;
    fluffle->lock.unlock();
  }
}
} // namespace

void Bigwig::try_merge() {
  if (fluffle_->merge) {
    fluffle_->lock.lock();
    if (fluffle_->workers < fluffle_->max_workers) {
      fluffle_->workers++;
      std::thread t(merge_worker, fluffle_);
      t.detach();
    }
    fluffle_->lock.unlock();
  }
}

void Bigwig::commit_() {
  fluffle_->lock.lock();
  fiver_->commit();
  fiver_->start();
  fluffle_->lock.unlock();
  appender_ = nullptr;
  annotator_ = nullptr;
  fiver_ = nullptr;
  try_merge();
}

void Bigwig::abort_() {
  fluffle_->lock.lock();
  fiver_->abort();
  fiver_->start();
  fluffle_->lock.unlock();
  appender_ = nullptr;
  annotator_ = nullptr;
  fiver_ = nullptr;
  try_merge();
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
