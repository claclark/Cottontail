#include "src/bigwig.h"

#include <algorithm>
#include <cassert>
#include <map>
#include <memory>
#include <regex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "src/annotator.h"
#include "src/appender.h"
#include "src/array_hopper.h"
#include "src/compressor.h"
#include "src/core.h"
#include "src/featurizer.h"
#include "src/fluffle.h"
#include "src/hazel.h"
#include "src/hopper.h"
#include "src/recipe.h"
#include "src/warren.h"

namespace cottontail {

class BigwigAnnotator final : public Annotator {
public:
  static std::shared_ptr<Annotator> make(std::shared_ptr<Fiver> fiver,
                                         std::string *error = nullptr) {
    if (fiver == nullptr) {
      safe_error(error) = "BigwigAnnotator got null Fiver";
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
      safe_error(error) = "BigwigAppender got null Fiver";
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
  make(const std::vector<std::shared_ptr<Owsla>> &warrens,
       std::shared_ptr<OwslaCache> cache,
       std::shared_ptr<SimplePostingFactory> posting_factory,
       addr text_chunk_feature) {
    std::shared_ptr<BigwigIdx> idx =
        std::shared_ptr<BigwigIdx>(new BigwigIdx());
    assert(idx != nullptr);
    idx->warrens_ = warrens;
    if (cache == nullptr)
      cache = std::make_shared<OwslaCache>();
    idx->cache_ = cache;
    assert(posting_factory != nullptr);
    idx->posting_factory_ = posting_factory;
    idx->text_chunk_feature_ = text_chunk_feature;
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
          std::move(raw_hopper(feature)), std::move(raw_hopper(null_feature)));
    } else {
      return raw_hopper(feature);
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
  std::vector<std::shared_ptr<Owsla>> contributors(addr feature) {
    std::vector<std::shared_ptr<Owsla>> contributing;
    for (auto &warren : warrens_)
      if (warren != nullptr && warren->idx()->count(feature) > 0)
        contributing.push_back(warren);
    return contributing;
  }

  static void
  fill_posting(std::shared_ptr<SimplePosting> posting,
               std::shared_ptr<SimplePostingFactory> posting_factory,
               std::vector<std::shared_ptr<Owsla>> contributing, addr feature) {
    std::vector<std::shared_ptr<SimplePosting>> postings;
    addr n = 0;
    for (auto &warren : contributing) {
      n += warren->idx()->count(feature);
      auto child = warren->posting(feature);
      if (child != nullptr)
        postings.push_back(child);
    }
    std::shared_ptr<SimplePosting> merged =
        posting_factory->posting_from_merge(postings);
    if (merged != nullptr) {
      posting->append(merged);
      posting->release();
      return;
    }
    assert(false);
    for (addr i = 0; i < n; i++)
      posting->push(minfinity + 1 + i, minfinity + 2 + i, 0.0);
    posting->release();
  }

  std::unique_ptr<Hopper> raw_hopper(addr feature) {
    std::vector<std::shared_ptr<Owsla>> contributing = contributors(feature);
    if (contributing.size() == 0)
      return std::make_unique<EmptyHopper>();
    if (contributing.size() == 1)
      return contributing[0]->idx()->hopper(feature);
    if (feature == text_chunk_feature_) {
      std::shared_ptr<SimplePosting> posting =
          posting_factory_->posting_from_feature(feature);
      fill_posting(posting, posting_factory_, contributing, feature);
      return ArrayHopper::make(posting);
    }
    bool fill;
    std::shared_ptr<SimplePosting> posting =
        cache_->get(feature, posting_factory_, &fill);
    if (fill) {
      std::shared_ptr<SimplePostingFactory> posting_factory = posting_factory_;
      std::thread thread([posting, posting_factory, contributing, feature] {
        fill_posting(posting, posting_factory, contributing, feature);
      });
      thread.detach();
    }
    return ArrayHopper::make(posting);
  }

  std::shared_ptr<OwslaCache> cache_;
  std::shared_ptr<SimplePostingFactory> posting_factory_;
  std::vector<std::shared_ptr<Owsla>> warrens_;
  addr text_chunk_feature_;
  bool erasing_;
};

class BigwigTxt final : public Txt {
public:
  static std::shared_ptr<Txt>
  make(const std::vector<std::shared_ptr<Owsla>> &warrens) {
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
  std::vector<std::shared_ptr<Owsla>> warrens_;
};

namespace {
struct SanitizedInventory {
  std::vector<OwslaShard> fivers;
  std::vector<OwslaShard> hazels;
  std::vector<HazelMergeRecovery> hazel_merges;
};

bool verify_shard_order(const std::string &kind,
                        const std::vector<OwslaShard> &shards,
                        std::string *error) {
  for (size_t i = 0; i < shards.size(); i++) {
    if (shards[i].start < 0 || shards[i].end < shards[i].start) {
      safe_error(error) = "Bad " + kind + " shard range: " + shards[i].name;
      return false;
    }
    if (i > 0 && shards[i - 1].end >= shards[i].start) {
      safe_error(error) = "Overlapping " + kind + " shards around: " +
                          shards[i].name;
      return false;
    }
  }
  return true;
}

bool combine_shards(std::shared_ptr<Working> working,
                    SanitizedInventory *inventory, std::string *error) {
  if (inventory == nullptr)
    return true;
  if (!verify_shard_order("fiver", inventory->fivers, error) ||
      !verify_shard_order("hazel", inventory->hazels, error))
    return false;
  if (inventory->fivers.empty() || inventory->hazels.empty())
    return true;

  OwslaShard last_hazel = inventory->hazels.back();
  while (!inventory->fivers.empty() &&
         owsla_ranges_overlap(last_hazel, inventory->fivers.front())) {
    OwslaShard fiver = inventory->fivers.front();
    if (!owsla_range_contains(last_hazel, fiver)) {
      safe_error(error) = "Mixed fiver/hazel sequence error around: " +
                          fiver.name + " and " + last_hazel.name;
      return false;
    }
    if (working != nullptr && !working->remove(fiver.name, error))
      return false;
    inventory->fivers.erase(inventory->fivers.begin());
  }

  if (!inventory->fivers.empty() &&
      inventory->fivers.front().end < last_hazel.start) {
    safe_error(error) = "Fiver shard precedes Hazel prefix: " +
                        inventory->fivers.front().name;
    return false;
  }
  return true;
}

bool sanitize(std::shared_ptr<Working> working, SanitizedInventory *inventory,
              std::string *error) {
  SanitizedInventory found;
  if (!Fiver::sanitize(working, &found.fivers, error) ||
      !Hazel::sanitize(working, &found.hazels, &found.hazel_merges, error) ||
      !combine_shards(working, &found, error))
    return false;
  if (inventory != nullptr)
    *inventory = found;
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
    safe_error(error) = "Burrow already has cottontail dna";
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
  std::string do_merge;
  if (parameters.find("parameters") != parameters.end()) {
    if (!cook(parameters["parameters"], &extra_parameters, error))
      return nullptr;
    auto container_element = extra_parameters.find("container");
    if (container_element != extra_parameters.end())
      container_query = container_element->second;
    std::string stemmer_name, stemmer_recipe;
    auto stemmer_element = extra_parameters.find("stemmer");
    if (stemmer_element != extra_parameters.end())
      stemmer_name = stemmer_element->second;
    if (stemmer_name != "") {
      stemmer = Stemmer::make(stemmer_name, stemmer_recipe, error);
      if (stemmer == nullptr)
        return nullptr;
    }
    auto do_merge_element = extra_parameters.find("merge");
    if (do_merge_element != extra_parameters.end())
      do_merge = do_merge_element->second;
  }
  std::shared_ptr<Fluffle> fluffle = Fluffle::make();
  (*fluffle->parameters) = extra_parameters;
  fluffle->merge = (do_merge == "" || okay(do_merge));
  SanitizedInventory inventory;
  if (!sanitize(working, &inventory, error))
    return nullptr;
  fluffle->hazel_merges = inventory.hazel_merges;
  std::vector<std::shared_ptr<Owsla>> visible;
  for (auto &hazelfile : inventory.hazels) {
    std::string hazelname = working->make_name(hazelfile.name);
    std::shared_ptr<Warren> warren = Warren::make(hazelname, error);
    if (warren == nullptr)
      return nullptr;
    std::shared_ptr<Hazel> hazel = std::dynamic_pointer_cast<Hazel>(warren);
    if (hazel == nullptr) {
      safe_error(error) = "Bigwig got non-Hazel shard: " + hazelfile.name;
      return nullptr;
    }
    hazel->start();
    visible.push_back(hazel);
    fluffle->warrens.push_back(hazel);
  }
  for (auto &fiverfile : inventory.fivers) {
    std::string fivername = working->make_name(fiverfile.name);
    std::shared_ptr<Fiver> fiver =
        Fiver::unpickle(fivername, working, featurizer, tokenizer, error,
                        posting_compressor, fvalue_compressor, text_compressor);
    if (fiver == nullptr)
      return nullptr;
    fiver->start();
    visible.push_back(fiver);
    fluffle->warrens.push_back(fiver);
  }
  addr address = 0;
  addr sequence = 0;
  if (visible.size() > 0) {
    addr p, q;
    for (size_t i = visible.size(); i > 0; --i) {
      if (visible[i - 1]->txt()->range(&p, &q)) {
        address = q + 1;
        break;
      }
    }
    if (!inventory.fivers.empty()) {
      sequence = inventory.fivers.back().end + 1;
    } else {
      sequence = inventory.hazels.back().end + 1;
    }
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
  bigwig->posting_factory_ =
      SimplePostingFactory::make(posting_compressor, fvalue_compressor);
  bigwig->text_compressor_ = text_compressor;
  bigwig->txt_recipe_ = txt_parameters["recipe"];
  if (stemmer != nullptr)
    bigwig->set_stemmer(stemmer);
  if (container_query != "")
    bigwig->set_default_container(container_query);
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
      safe_error(error) = "Burrow already has cottontail dna";
      return nullptr;
    }
  }
  if (featurizer == nullptr) {
    safe_error(error) = "Bigwig needs a featurizer (got nullptr)";
    return nullptr;
  }
  if (tokenizer == nullptr) {
    safe_error(error) = "Bigwig needs a tokenizer (got nullptr)";
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
  bigwig->posting_factory_ =
      SimplePostingFactory::make(bigwig->posting_compressor_,
                                 bigwig->fvalue_compressor_);
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
  bigwig->posting_factory_ = posting_factory_;
  bigwig->text_compressor_ = text_compressor_;
  bigwig->default_container_ = default_container_;
  if (stemmer_ != nullptr) {
    std::shared_ptr<cottontail::Stemmer> the_stemmer =
        cottontail::Stemmer::make(stemmer_->name(), stemmer_->recipe(), error);
    if (the_stemmer == nullptr)
      return nullptr;
    bigwig->stemmer_ = the_stemmer;
  }
  warrens_lock_.lock();
  if (warrens_valid_) {
    assert(cache_ != nullptr);
    for (auto &warren : warrens_)
      bigwig->warrens_.push_back(warren);
    bigwig->cache_ = cache_;
    bigwig->warrens_valid_ = true;
    warrens_lock_.unlock();
    bigwig->start();
  } else {
    warrens_lock_.unlock();
  }
  return bigwig;
}

void Bigwig::start_() {
  std::shared_ptr<OwslaCache> cache;
  {
    std::lock_guard<std::mutex> _(warrens_lock_);
    if (!warrens_valid_) {
      fluffle_->lock.lock();
      warrens_.clear();
      for (auto &warren : fluffle_->warrens)
        if (warren != nullptr &&
            (warren->name() == "hazel" || warren->name() == "fiver"))
          warrens_.push_back(warren);
      if (fluffle_->cache == nullptr)
        fluffle_->cache = std::make_shared<OwslaCache>();
      cache_ = fluffle_->cache;
      fluffle_->lock.unlock();
      warrens_valid_ = true;
    }
    assert(cache_ != nullptr);
    cache = cache_;
  }
  idx_ = BigwigIdx::make(warrens_, cache, posting_factory_,
                         featurizer_->featurize(text_chunk_tag));
  assert(idx_ != nullptr);
  txt_ = Txt::wrap(txt_recipe_, BigwigTxt::make(warrens_));
  assert(txt_ != nullptr);
}

void Bigwig::end_() {
  {
    std::lock_guard<std::mutex> _(warrens_lock_);
    warrens_valid_ = false;
    warrens_.clear();
    cache_ = nullptr;
  }
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
#if 1
bool find_sequence(const std::vector<bool> a, size_t *start, size_t *end) {
  size_t best_len = 0;
  size_t best_start = 0;
  size_t cur_start = 0;
  size_t cur_len = 0;
  for (size_t i = 0; i < a.size(); ++i) {
    if (a[i]) {
      if (cur_len == 0)
        cur_start = i;
      ++cur_len;
    } else {
      if (cur_len >= 3 && cur_len > best_len) { // ← only change
        best_len = cur_len;
        best_start = cur_start;
      }
      cur_len = 0;
    }
  }
  if (cur_len >= 3 && cur_len > best_len) {
    best_len = cur_len;
    best_start = cur_start;
  }
  if (best_len >= 3 && start && end) {
    *start = best_start;
    *end = best_start + best_len - 1;
    return true;
  }
  return false;
}

bool find_smallest_pair(const std::vector<addr> a, size_t *start, size_t *end) {
  if (a.size() < 2)
    return false;
  bool found = false;
  size_t best_i = 0;
  addr best_sum = 0;
  for (size_t i = 0; i + 1 < a.size(); ++i) {
    const addr x = a[i];
    const addr y = a[i + 1];
    if (x >= 0 && y >= 0) {
      const addr s = x + y;
      if (!found || s < best_sum) {
        found = true;
        best_sum = s;
        best_i = i;
      }
    }
  }
  if (found && start && end) {
    *start = best_i;
    *end = best_i + 1;
  }
  return found;
}

void merge_worker(std::shared_ptr<Fluffle> fluffle) {
  for (;;) {
    std::vector<std::shared_ptr<Fiver>> fivers;
    std::shared_ptr<Owsla> start_warren;
    std::shared_ptr<Owsla> end_warren;
    {
      std::lock_guard<std::mutex> _(fluffle->lock);
      bool cleanup = false;
      for (auto &warren : fluffle->warrens)
        if (warren->name() == "remove") {
          cleanup = true;
          break;
        }
      if (cleanup) {
        std::vector<std::shared_ptr<Owsla>> warrens;
        for (auto &warren : fluffle->warrens)
          if (warren->name() != "remove")
            warrens.push_back(warren);
        fluffle->warrens = warrens;
      }
      if (fluffle->warrens.size() < 2)
        break;
      std::vector<bool> small;
      std::vector<addr> storage;
      for (size_t i = 0; i < fluffle->warrens.size(); i++) {
        if (fluffle->warrens[i] != nullptr &&
            fluffle->warrens[i]->name() == "fiver" &&
            fluffle->merging.find(fluffle->warrens[i]) ==
                fluffle->merging.end()) {
          addr n = fluffle->warrens[i]->estimated_size();
          small.push_back(n < 4 * 1024 * 1024); // arbitrary
          storage.push_back(n);
        } else {
          small.push_back(false);
          storage.push_back(-1);
        }
      }
      size_t start, end;
      if (!find_sequence(small, &start, &end) &&
          !find_smallest_pair(storage, &start, &end))
        break;
      for (size_t i = start; i <= end; i++) {
        fivers.push_back(std::static_pointer_cast<Fiver>(fluffle->warrens[i]));
        fluffle->merging.insert(fluffle->warrens[i]);
      }
#if 0
      {
        std::cerr << "Merging:\n";
        for (auto &fiver : fivers) {
          addr sequence_start, sequence_end;
          fiver->get_sequence(&sequence_start, &sequence_end);
          std::cerr << " fiver." << sequence_start << "." << sequence_end
                    << " (" << fiver->get_storage_estimate() << ")\n";
        }
      }
#endif
      if (fluffle->workers < fluffle->max_workers) {
        fluffle->workers++;
        std::thread t(merge_worker, fluffle);
        t.detach();
      }
      start_warren = fluffle->warrens[start];
      end_warren = fluffle->warrens[end];
    }
    std::shared_ptr<Fiver> merged = Fiver::merge(fivers);
    merged->pickle();
    for (auto &fiver : fivers)
      fiver->discard();
    {
      std::lock_guard<std::mutex> _(fluffle->lock);
      std::vector<std::shared_ptr<Owsla>> warrens;
      size_t i;
      for (i = 0;
           i < fluffle->warrens.size() && fluffle->warrens[i] != start_warren;
           i++)
        warrens.push_back(fluffle->warrens[i]);
      assert(i < fluffle->warrens.size());
      if (merged != nullptr) {
        merged->start();
        warrens.push_back(merged);
      }
      for (; i < fluffle->warrens.size() && fluffle->warrens[i] != end_warren;
           i++)
        fluffle->merging.erase(fluffle->warrens[i]);
      ;
      assert(i < fluffle->warrens.size());
      fluffle->merging.erase(fluffle->warrens[i]);
      for (i++; i < fluffle->warrens.size(); i++)
        warrens.push_back(fluffle->warrens[i]);
      fluffle->warrens = warrens;
    }
  }
  {
    std::lock_guard<std::mutex> _(fluffle->lock);
    assert(fluffle->workers > 0);
    --fluffle->workers;
  }
}

#else
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
#endif
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
  fluffle_->cache = std::make_shared<OwslaCache>();
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
