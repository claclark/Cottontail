#include "src/bigwig.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
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
const addr small_shard = 8 * 1024 * 1024;
const addr medium_shard = 256 * 1024 * 1024;
const addr large_shard = 2 * medium_shard;

struct SanitizedInventory {
  std::vector<OwslaShard> fivers;
  std::vector<OwslaShard> hazels;
  std::vector<OwslaShard> shards;
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
      safe_error(error) =
          "Overlapping " + kind + " shards around: " + shards[i].name;
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

  std::vector<OwslaShard> fivers;
  size_t hazel = 0;
  for (auto &fiver : inventory->fivers) {
    while (hazel < inventory->hazels.size() &&
           inventory->hazels[hazel].end < fiver.start)
      hazel++;
    if (hazel < inventory->hazels.size() &&
        owsla_ranges_overlap(inventory->hazels[hazel], fiver)) {
      if (owsla_range_contains(inventory->hazels[hazel], fiver)) {
        if (working != nullptr && !working->remove(fiver.name, error))
          return false;
        continue;
      }
      if (owsla_range_contains(fiver, inventory->hazels[hazel])) {
        safe_error(error) =
            "Fiver shard contains Hazel shard around: " + fiver.name + " and " +
            inventory->hazels[hazel].name;
      } else {
        safe_error(error) = "Mixed fiver/hazel overlap around: " + fiver.name +
                            " and " + inventory->hazels[hazel].name;
      }
      return false;
    }
    fivers.push_back(fiver);
  }
  inventory->fivers = fivers;

  inventory->shards.clear();
  inventory->shards.reserve(inventory->hazels.size() +
                            inventory->fivers.size());
  inventory->shards.insert(inventory->shards.end(), inventory->hazels.begin(),
                           inventory->hazels.end());
  inventory->shards.insert(inventory->shards.end(), inventory->fivers.begin(),
                           inventory->fivers.end());
  std::sort(inventory->shards.begin(), inventory->shards.end());
  return verify_shard_order("combined", inventory->shards, error);
}

bool same_shard(const OwslaShard &a, const OwslaShard &b) {
  return a.start == b.start && a.end == b.end && a.name == b.name;
}

bool recovery_matches_inventory(const HazelMergeRecovery &recovery,
                                const std::vector<OwslaShard> &shards) {
  if (recovery.sources.size() < 2 ||
      recovery.sources.front().start != recovery.target.start ||
      recovery.sources.back().end != recovery.target.end)
    return false;
  for (size_t first = 0; first < shards.size(); first++) {
    if (!same_shard(shards[first], recovery.sources.front()))
      continue;
    if (first + recovery.sources.size() > shards.size())
      return false;
    for (size_t i = 0; i < recovery.sources.size(); i++)
      if (!same_shard(shards[first + i], recovery.sources[i]))
        return false;
    return true;
  }
  return false;
}

bool reconcile_hazel_merges(std::shared_ptr<Working> working,
                            SanitizedInventory *inventory, std::string *error) {
  std::vector<HazelMergeRecovery> coherent;
  for (auto &recovery : inventory->hazel_merges) {
    if (recovery_matches_inventory(recovery, inventory->shards)) {
      coherent.push_back(recovery);
    } else if (!remove_hazel_merge_segments(working, recovery, error)) {
      return false;
    }
  }

  std::vector<bool> conflicting(coherent.size(), false);
  for (size_t i = 0; i < coherent.size(); i++)
    for (size_t j = i + 1; j < coherent.size(); j++)
      if (owsla_ranges_overlap(coherent[i].target, coherent[j].target)) {
        conflicting[i] = true;
        conflicting[j] = true;
      }

  inventory->hazel_merges.clear();
  for (size_t i = 0; i < coherent.size(); i++) {
    if (conflicting[i]) {
      if (!remove_hazel_merge_segments(working, coherent[i], error))
        return false;
    } else {
      inventory->hazel_merges.push_back(coherent[i]);
    }
  }
  std::sort(inventory->hazel_merges.begin(), inventory->hazel_merges.end());
  return true;
}

std::string shell_quote(const std::string &s) {
  std::string quoted = "'";
  for (char c : s) {
    if (c == '\'')
      quoted += "'\\''";
    else
      quoted += c;
  }
  quoted += "'";
  return quoted;
}

bool is_commit_script(const std::string &name) {
  return name.size() > 10 && name.compare(0, 7, "commit.") == 0 &&
         name.compare(name.size() - 3, 3, ".sh") == 0;
}

bool finalize_commits(std::shared_ptr<Working> working, std::string *error) {
  if (working == nullptr)
    return true;
  for (auto &name : working->ls("commit")) {
    if (!is_commit_script(name))
      continue;
    std::string command = shell_quote(working->make_name(name));
    if (std::system(command.c_str()) != 0) {
      safe_error(error) = "Could not finalize commit script: " + name;
      return false;
    }
    if (!working->remove(name, error))
      return false;
  }
  return true;
}

bool sanitize(std::shared_ptr<Working> working, SanitizedInventory *inventory,
              std::string *error) {
  SanitizedInventory found;
  if (!finalize_commits(working, error) ||
      !Fiver::sanitize(working, &found.fivers, error) ||
      !Hazel::sanitize(working, &found.hazels, &found.hazel_merges, error) ||
      !combine_shards(working, &found, error) ||
      !reconcile_hazel_merges(working, &found, error))
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

struct BigwigContext {
  std::shared_ptr<Working> working;
  std::shared_ptr<Featurizer> featurizer;
  std::shared_ptr<Tokenizer> tokenizer;
  std::shared_ptr<Compressor> posting_compressor;
  std::shared_ptr<Compressor> fvalue_compressor;
  std::shared_ptr<Compressor> text_compressor;
  std::shared_ptr<Stemmer> stemmer;
  std::map<std::string, std::string> parameters;
  std::string container_query;
  std::string txt_recipe;
  SanitizedInventory inventory;
};

bool load_bigwig_context(const std::string &burrow, BigwigContext *context,
                         std::string *error) {
  if (context == nullptr) {
    safe_error(error) = "Bigwig needs a context";
    return false;
  }
  std::string the_burrow = burrow;
  if (the_burrow == "")
    the_burrow = DEFAULT_BURROW;
  BigwigContext found;
  found.working = Working::make(the_burrow, error);
  if (found.working == nullptr)
    return false;
  std::string dna;
  if (!read_dna(found.working, &dna, error))
    return false;
  std::map<std::string, std::string> dna_parameters;
  if (!cook(dna, &dna_parameters, error))
    return false;
  std::map<std::string, std::string> featurizer_parameters;
  if (!cook(dna_parameters["featurizer"], &featurizer_parameters, error))
    return false;
  found.featurizer =
      Featurizer::make(featurizer_parameters["name"],
                       featurizer_parameters["recipe"], error, found.working);
  if (found.featurizer == nullptr)
    return false;
  std::map<std::string, std::string> tokenizer_parameters;
  if (!cook(dna_parameters["tokenizer"], &tokenizer_parameters, error))
    return false;
  found.tokenizer = Tokenizer::make(tokenizer_parameters["name"],
                                    tokenizer_parameters["recipe"], error);
  if (found.tokenizer == nullptr)
    return false;
  std::map<std::string, std::string> idx_parameters;
  if (!cook(dna_parameters["idx"], &idx_parameters, error))
    return false;
  std::map<std::string, std::string> idx_recipe_parameters;
  if (!cook(idx_parameters["recipe"], &idx_recipe_parameters, error))
    return false;
  found.posting_compressor = Compressor::make(
      idx_recipe_parameters["posting_compressor"],
      idx_recipe_parameters["posting_compressor_recipe"], error);
  if (found.posting_compressor == nullptr)
    return false;
  found.fvalue_compressor = Compressor::make(
      idx_recipe_parameters["fvalue_compressor"],
      idx_recipe_parameters["fvalue_compressor_recipe"], error);
  if (found.fvalue_compressor == nullptr)
    return false;
  std::map<std::string, std::string> txt_parameters;
  if (!cook(dna_parameters["txt"], &txt_parameters, error))
    return false;
  std::map<std::string, std::string> txt_recipe_parameters;
  if (!cook(txt_parameters["recipe"], &txt_recipe_parameters, error))
    return false;
  found.text_compressor =
      Compressor::make(txt_recipe_parameters["compressor"],
                       txt_recipe_parameters["compressor_recipe"], error);
  if (found.text_compressor == nullptr)
    return false;
  found.txt_recipe = txt_parameters["recipe"];
  if (dna_parameters.find("parameters") != dna_parameters.end()) {
    if (!cook(dna_parameters["parameters"], &found.parameters, error))
      return false;
    auto container_element = found.parameters.find("container");
    if (container_element != found.parameters.end())
      found.container_query = container_element->second;
    auto stemmer_element = found.parameters.find("stemmer");
    if (stemmer_element != found.parameters.end() &&
        stemmer_element->second != "") {
      found.stemmer = Stemmer::make(stemmer_element->second, "", error);
      if (found.stemmer == nullptr)
        return false;
    }
  }
  if (!sanitize(found.working, &found.inventory, error))
    return false;
  *context = found;
  return true;
}

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
  BigwigContext context;
  if (!load_bigwig_context(burrow, &context, error))
    return nullptr;
  std::shared_ptr<Fluffle> fluffle = Fluffle::make();
  fluffle->working = context.working;
  (*fluffle->parameters) = context.parameters;
  fluffle->hazel_merges = context.inventory.hazel_merges;
  std::vector<std::shared_ptr<Owsla>> visible;
  for (auto &shard : context.inventory.shards) {
    if (shard.name.compare(0, 6, "hazel.") == 0) {
      std::string hazelname = context.working->make_name(shard.name);
      std::shared_ptr<Warren> warren = Warren::make(hazelname, error);
      if (warren == nullptr)
        return nullptr;
      std::shared_ptr<Hazel> hazel = std::dynamic_pointer_cast<Hazel>(warren);
      if (hazel == nullptr) {
        safe_error(error) = "Bigwig got non-Hazel shard: " + shard.name;
        return nullptr;
      }
      hazel->start();
      visible.push_back(hazel);
      fluffle->warrens.push_back(hazel);
    } else if (shard.name.compare(0, 6, "fiver.") == 0) {
      std::string fivername = context.working->make_name(shard.name);
      std::shared_ptr<Fiver> fiver =
          Fiver::unpickle(fivername, context.working, context.featurizer,
                          context.tokenizer, error, context.posting_compressor,
                          context.fvalue_compressor, context.text_compressor);
      if (fiver == nullptr)
        return nullptr;
      fiver->start();
      visible.push_back(fiver);
      fluffle->warrens.push_back(fiver);
    } else {
      safe_error(error) = "Bigwig got unknown shard: " + shard.name;
      return nullptr;
    }
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
    sequence = context.inventory.shards.back().end + 1;
  }
  fluffle->address = address;
  fluffle->sequence = sequence;
  std::shared_ptr<Bigwig> bigwig =
      std::shared_ptr<Bigwig>(new Bigwig(context.working, context.featurizer,
                                         context.tokenizer, nullptr, nullptr));
  assert(bigwig != nullptr);
  bigwig->fiver_ = nullptr;
  bigwig->fluffle_ = fluffle;
  bigwig->posting_compressor_ = context.posting_compressor;
  bigwig->fvalue_compressor_ = context.fvalue_compressor;
  bigwig->posting_factory_ = SimplePostingFactory::make(
      context.posting_compressor, context.fvalue_compressor);
  bigwig->text_compressor_ = context.text_compressor;
  bigwig->txt_recipe_ = context.txt_recipe;
  if (context.stemmer != nullptr)
    bigwig->set_stemmer(context.stemmer);
  if (context.container_query != "")
    bigwig->set_default_container(context.container_query);
  bigwig->try_merge();
  return bigwig;
}

bool Bigwig::consolidate(const std::string &burrow, std::string *error,
                         bool verbose) {
  using SteadyClock = std::chrono::steady_clock;
  auto timestamp = []() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
  };
  auto elapsed = [](const SteadyClock::time_point &start) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               SteadyClock::now() - start)
        .count();
  };
  auto report = [&](const std::string &message) {
    if (verbose)
      std::cerr << "Bigwig::consolidate [" << timestamp() << " ms]: " << message
                << "\n";
  };

  auto total_start = SteadyClock::now();
  auto phase_start = SteadyClock::now();
  report("Opening and sanitizing " +
         (burrow == "" ? std::string(DEFAULT_BURROW) : burrow) + "...");
  BigwigContext context;
  if (!load_bigwig_context(burrow, &context, error))
    return false;
  report("Opening and sanitizing took " + std::to_string(elapsed(phase_start)) +
         " ms");

  if (context.inventory.shards.empty()) {
    safe_error(error) = "Bigwig consolidation found no shards";
    return false;
  }

  addr sequence_start = context.inventory.shards.front().start;
  addr sequence_end = context.inventory.shards.back().end;
  std::string parameters = freeze(context.parameters);

  struct FiverGroup {
    std::vector<OwslaShard> shards;
    std::vector<std::shared_ptr<Fiver>> fivers;
  };
  std::vector<FiverGroup> groups;
  std::vector<std::string> hazels;
  phase_start = SteadyClock::now();
  report("Loading " + std::to_string(context.inventory.fivers.size()) +
         " Fiver(s)...");
  for (size_t i = 0; i < context.inventory.shards.size();) {
    auto &shard = context.inventory.shards[i];
    if (shard.name.compare(0, 6, "hazel.") == 0) {
      hazels.push_back(shard.name);
      i++;
      continue;
    }
    if (shard.name.compare(0, 6, "fiver.") != 0) {
      safe_error(error) =
          "Bigwig consolidation found unknown shard: " + shard.name;
      return false;
    }

    size_t end = i;
    while (end < context.inventory.shards.size() &&
           context.inventory.shards[end].name.compare(0, 6, "fiver.") == 0)
      end++;

    FiverGroup group;
    addr group_size = 0;
    for (size_t j = i; j < end; j++) {
      auto &fiver_shard = context.inventory.shards[j];
      std::shared_ptr<Fiver> fiver =
          Fiver::unpickle(fiver_shard.name, context.working, context.featurizer,
                          context.tokenizer, error, context.posting_compressor,
                          context.fvalue_compressor, context.text_compressor);
      if (fiver == nullptr)
        return false;
      addr estimate = fiver->estimated_size();
      if (estimate < 0) {
        safe_error(error) =
            "Bigwig consolidation got bad Fiver estimate: " + fiver_shard.name;
        return false;
      }
      group.shards.push_back(fiver_shard);
      group.fivers.push_back(fiver);
      if (estimate > maxfinity - group_size)
        group_size = maxfinity;
      else
        group_size += estimate;
      if (group_size >= medium_shard) {
        hazels.push_back(hazel_default_name(group.shards.front().start,
                                            group.shards.back().end));
        groups.push_back(std::move(group));
        group = FiverGroup();
        group_size = 0;
      }
    }
    if (!group.fivers.empty()) {
      hazels.push_back(hazel_default_name(group.shards.front().start,
                                          group.shards.back().end));
      groups.push_back(std::move(group));
    }
    i = end;
  }
  report("Loading Fivers took " + std::to_string(elapsed(phase_start)) + " ms");

  if (!groups.empty()) {
    size_t worker_count =
        std::min(groups.size(), std::max<size_t>(1, allowed_threads(0)));
    phase_start = SteadyClock::now();
    report("Merging and converting " + std::to_string(groups.size()) +
           " Fiver group(s) with " + std::to_string(worker_count) +
           " worker(s)...");

    std::atomic<size_t> next_group(0);
    std::atomic<bool> failed(false);
    std::mutex failure_lock;
    std::string failure;
    auto fail = [&](const std::string &message) {
      bool expected = false;
      if (failed.compare_exchange_strong(expected, true)) {
        std::lock_guard<std::mutex> lock(failure_lock);
        failure = message;
      }
    };
    auto work = [&]() {
      while (!failed.load()) {
        size_t index = next_group.fetch_add(1);
        if (index >= groups.size())
          return;
        auto &group = groups[index];
        std::string group_error;
        try {
          std::shared_ptr<Fiver> merged;
          if (group.fivers.size() == 1)
            merged = group.fivers.front();
          else
            merged = Fiver::merge(group.fivers, &group_error);
          if (merged == nullptr) {
            fail(group_error == "" ? "Bigwig consolidation failed to merge "
                                     "a Fiver group"
                                   : group_error);
            return;
          }

          merged->start();
          std::shared_ptr<Hazel> hazel =
              merged->hazel(&group_error, 64 * 1024, parameters);
          merged->end();
          if (hazel == nullptr) {
            fail(group_error == "" ? "Bigwig consolidation failed to convert "
                                     "a Fiver group"
                                   : group_error);
            return;
          }

          for (auto &shard : group.shards)
            if (!context.working->remove(shard.name, &group_error)) {
              fail(group_error);
              return;
            }
          hazel.reset();
          merged.reset();
          group.fivers.clear();
        } catch (const std::exception &exception) {
          fail("Bigwig consolidation worker failed: " +
               std::string(exception.what()));
          return;
        } catch (...) {
          fail("Bigwig consolidation worker failed");
          return;
        }
      }
    };

    std::vector<std::thread> workers;
    workers.reserve(worker_count);
    try {
      for (size_t i = 0; i < worker_count; i++)
        workers.emplace_back(work);
    } catch (const std::exception &exception) {
      fail("Bigwig consolidation can't start worker: " +
           std::string(exception.what()));
    } catch (...) {
      fail("Bigwig consolidation can't start worker");
    }
    for (auto &worker : workers)
      worker.join();
    if (failed.load()) {
      std::lock_guard<std::mutex> lock(failure_lock);
      safe_error(error) = failure;
      return false;
    }
    report("Fiver merging and conversion took " +
           std::to_string(elapsed(phase_start)) + " ms; removed " +
           std::to_string(context.inventory.fivers.size()) +
           " source Fiver(s)");
  }

  size_t discarded_recoveries = 0;
  for (auto &recovery : context.inventory.hazel_merges) {
    bool reusable = recovery.target.start == sequence_start &&
                    recovery.target.end == sequence_end &&
                    recovery.sources.size() == hazels.size();
    for (size_t i = 0; reusable && i < hazels.size(); i++)
      reusable = recovery.sources[i].name == hazels[i];
    if (!reusable) {
      if (!remove_hazel_merge_segments(context.working, recovery, error))
        return false;
      discarded_recoveries++;
    }
  }
  if (discarded_recoveries > 0)
    report("Discarded " + std::to_string(discarded_recoveries) +
           " unused partial Hazel merge(s)");

  if (hazels.size() > 1) {
    phase_start = SteadyClock::now();
    report("Merging " + std::to_string(hazels.size()) + " Hazel(s)...");
    if (!Hazel::merge(context.working, hazels, parameters, error))
      return false;
    report("Hazel merge took " + std::to_string(elapsed(phase_start)) + " ms");
    for (auto &hazel : hazels)
      if (!context.working->remove(hazel, error))
        return false;
    report("Removed " + std::to_string(hazels.size()) + " source Hazel(s)");
  }

  phase_start = SteadyClock::now();
  report("Sanitizing consolidated shards...");
  SanitizedInventory final_inventory;
  if (!sanitize(context.working, &final_inventory, error))
    return false;
  report("Final sanitization took " + std::to_string(elapsed(phase_start)) +
         " ms");
  if (!final_inventory.fivers.empty() || final_inventory.hazels.size() != 1 ||
      final_inventory.shards.size() != 1 ||
      final_inventory.hazels[0].start != sequence_start ||
      final_inventory.hazels[0].end != sequence_end) {
    safe_error(error) =
        "Bigwig consolidation did not produce one complete Hazel";
    return false;
  }
  report("Consolidation took " + std::to_string(elapsed(total_start)) + " ms");
  return true;
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
  bigwig->fluffle_->working = working;
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
  bigwig->posting_factory_ = SimplePostingFactory::make(
      bigwig->posting_compressor_, bigwig->fvalue_compressor_);
  if (working != nullptr && !write_dna(working, bigwig->recipe(), error))
    return nullptr;
  return bigwig;
}

void Bigwig::merge(bool on, bool convert) {
  if (on)
    set_parameter("convert", okay(convert));
  set_parameter("merge", okay(on));
  if (on)
    try_merge();
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

bool Bigwig::ready_(std::string *error) {
  fluffle_->lock.lock();
  fluffle_->address = fiver_->relocate(fluffle_->address);
  fiver_->set_sequence(fluffle_->sequence);
  fluffle_->sequence++;
  fluffle_->warrens.push_back(fiver_);
  fluffle_->lock.unlock();
  return fiver_->ready(error);
}

namespace {
// The caller holds fluffle->lock while consulting live policy parameters.
bool fluffle_parameter_enabled(std::shared_ptr<Fluffle> fluffle,
                               const std::string &key) {
  assert(fluffle != nullptr);
  if (fluffle->parameters == nullptr)
    return true;
  auto parameter = fluffle->parameters->find(key);
  return parameter == fluffle->parameters->end() || okay(parameter->second);
}

bool find_sequence(const std::vector<bool> &a, size_t *start, size_t *end) {
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
      if (cur_len >= 3 && cur_len > best_len) {
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

bool find_smallest_pair(const std::vector<addr> &a, size_t *start,
                        size_t *end) {
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

bool eligible(std::shared_ptr<Fluffle> fluffle, std::shared_ptr<Owsla> warren) {
  return warren != nullptr &&
         fluffle->merging.find(warren) == fluffle->merging.end();
}

bool hazel_merge_okay(std::shared_ptr<Fluffle> fluffle) {
  size_t merging_hazels = 0;
  for (auto &warren : fluffle->merging)
    if (warren != nullptr && warren->name() == "hazel")
      merging_hazels++;
  return merging_hazels + 1 < fluffle->max_workers;
}

bool find_tiny_fiver_run(std::shared_ptr<Fluffle> fluffle, size_t *start,
                         size_t *end) {
  std::vector<bool> tiny(fluffle->warrens.size(), false);
  for (size_t i = 0; i < fluffle->warrens.size(); i++) {
    auto warren = fluffle->warrens[i];
    tiny[i] = eligible(fluffle, warren) && warren->name() == "fiver" &&
              warren->estimated_size() < small_shard;
  }
  return find_sequence(tiny, start, end);
}

bool find_smallest_fiver_pair(std::shared_ptr<Fluffle> fluffle, size_t *start,
                              size_t *end) {
  bool convert = fluffle_parameter_enabled(fluffle, "convert");
  bool found = false;
  size_t best = 0;
  addr best_sum = 0;
  for (size_t i = 0; i + 1 < fluffle->warrens.size(); i++) {
    auto left = fluffle->warrens[i];
    auto right = fluffle->warrens[i + 1];
    if (!eligible(fluffle, left) || !eligible(fluffle, right) ||
        left->name() != "fiver" || right->name() != "fiver")
      continue;
    addr left_size = left->estimated_size();
    addr right_size = right->estimated_size();
    if (left_size < 0 || right_size < 0 ||
        (convert && (left_size >= medium_shard || right_size >= medium_shard)))
      continue;
    addr sum = left_size + right_size;
    if (!found || sum < best_sum) {
      found = true;
      best = i;
      best_sum = sum;
    }
  }
  if (!found)
    return false;
  if (start != nullptr && end != nullptr) {
    *start = best;
    *end = best + 1;
  }
  return true;
}

bool find_stranded_fiver_conversion(std::shared_ptr<Fluffle> fluffle,
                                    size_t *start, size_t *end) {
  if (!fluffle_parameter_enabled(fluffle, "convert"))
    return false;
  if (fluffle->warrens.size() >= 2) {
    auto warren = fluffle->warrens[0];
    auto right = fluffle->warrens[1];
    if (warren != nullptr && right != nullptr && warren->name() == "fiver" &&
        right->name() == "hazel" && eligible(fluffle, warren)) {
      if (start != nullptr && end != nullptr)
        *start = *end = 0;
      return true;
    }
  }
  if (fluffle->warrens.size() < 3)
    return false;
  for (size_t i = 1; i + 1 < fluffle->warrens.size(); i++) {
    auto left = fluffle->warrens[i - 1];
    auto warren = fluffle->warrens[i];
    auto right = fluffle->warrens[i + 1];
    if (left != nullptr && right != nullptr && warren != nullptr &&
        left->name() == "hazel" && warren->name() == "fiver" &&
        right->name() == "hazel" && eligible(fluffle, warren)) {
      if (start != nullptr && end != nullptr) {
        *start = i;
        *end = i;
      }
      return true;
    }
  }
  return false;
}

bool find_oldest_large_fiver_conversion(std::shared_ptr<Fluffle> fluffle,
                                        size_t *start, size_t *end) {
  if (!fluffle_parameter_enabled(fluffle, "convert"))
    return false;
  for (size_t i = 0; i < fluffle->warrens.size(); i++) {
    auto warren = fluffle->warrens[i];
    if (eligible(fluffle, warren) && warren->name() == "fiver" &&
        warren->estimated_size() >= medium_shard) {
      if (start != nullptr && end != nullptr) {
        *start = i;
        *end = i;
      }
      return true;
    }
  }
  return false;
}

bool same_sequence(const OwslaShard &shard, std::shared_ptr<Owsla> warren) {
  if (warren == nullptr || warren->name() != "hazel")
    return false;
  addr start;
  addr end;
  warren->get_sequence(&start, &end);
  return shard.start == start && shard.end == end;
}

bool find_recovered_hazel_merge(std::shared_ptr<Fluffle> fluffle, size_t *start,
                                size_t *end) {
  for (auto &recovery : fluffle->hazel_merges) {
    if (recovery.sources.size() < 2 ||
        recovery.sources.size() > fluffle->warrens.size())
      continue;
    for (size_t i = 0; i + recovery.sources.size() <= fluffle->warrens.size();
         i++) {
      bool match = true;
      for (size_t j = 0; j < recovery.sources.size(); j++) {
        auto warren = fluffle->warrens[i + j];
        if (!eligible(fluffle, warren) ||
            !same_sequence(recovery.sources[j], warren)) {
          match = false;
          break;
        }
      }
      if (match) {
        if (start != nullptr && end != nullptr) {
          *start = i;
          *end = i + recovery.sources.size() - 1;
        }
        return true;
      }
    }
  }
  return false;
}

bool find_smallest_hazel_pair(std::shared_ptr<Fluffle> fluffle, size_t *start,
                              size_t *end) {
  std::vector<addr> storage(fluffle->warrens.size(), -1);
  for (size_t i = 0; i < fluffle->warrens.size(); i++) {
    auto warren = fluffle->warrens[i];
    if (eligible(fluffle, warren) && warren->name() == "hazel")
      storage[i] = warren->estimated_size();
  }
  return find_smallest_pair(storage, start, end);
}

bool find_hazel_action(std::shared_ptr<Fluffle> fluffle, size_t *start,
                       size_t *end) {
  if (!hazel_merge_okay(fluffle))
    return false;
  if (find_recovered_hazel_merge(fluffle, start, end))
    return true;
  if (find_smallest_hazel_pair(fluffle, start, end))
    return true;
  return false;
}

bool find_lone_fiver_cleanup(std::shared_ptr<Fluffle> fluffle, size_t *start,
                             size_t *end) {
  if (!fluffle_parameter_enabled(fluffle, "convert"))
    return false;
  bool found_fiver = false;
  bool fiver_eligible = false;
  size_t fiver_index = 0;
  size_t hazels = 0;
  addr smallest_hazel = 0;
  for (size_t i = 0; i < fluffle->warrens.size(); i++) {
    auto warren = fluffle->warrens[i];
    if (warren == nullptr)
      continue;
    if (warren->name() == "fiver") {
      if (found_fiver)
        return false;
      found_fiver = true;
      fiver_eligible = eligible(fluffle, warren);
      fiver_index = i;
    } else if (warren->name() == "hazel") {
      addr size = warren->estimated_size();
      if (hazels == 0 || size < smallest_hazel)
        smallest_hazel = size;
      hazels++;
    } else {
      return false;
    }
  }
  if (!found_fiver || !fiver_eligible)
    return false;
  if (hazels == 1 || (hazels > 1 && smallest_hazel > large_shard)) {
    if (start != nullptr && end != nullptr) {
      *start = fiver_index;
      *end = fiver_index;
    }
    return true;
  }
  return false;
}

bool find_fiver_action(std::shared_ptr<Fluffle> fluffle, size_t *start,
                       size_t *end) {
  if (find_lone_fiver_cleanup(fluffle, start, end))
    return true;
  if (find_tiny_fiver_run(fluffle, start, end))
    return true;
  if (find_stranded_fiver_conversion(fluffle, start, end))
    return true;
  if (find_oldest_large_fiver_conversion(fluffle, start, end))
    return true;
  if (find_smallest_fiver_pair(fluffle, start, end))
    return true;
  return false;
}

bool find_merge_action(std::shared_ptr<Fluffle> fluffle, size_t *start,
                       size_t *end) {
  if (!fluffle_parameter_enabled(fluffle, "merge"))
    return false;
  if (find_fiver_action(fluffle, start, end))
    return true;
  if (find_hazel_action(fluffle, start, end))
    return true;
  return false;
}

enum class MergeAction { none, fiver_merge, hazel_merge, fiver_to_hazel };

bool validate_fiver_conversion(
    std::shared_ptr<Fluffle> fluffle,
    const std::vector<std::shared_ptr<Owsla>> &selected,
    std::shared_ptr<Fiver> *fiver_to_hazel, std::string *parameters) {
  if (selected.size() != 1 || selected[0] == nullptr ||
      selected[0]->name() != "fiver" || !eligible(fluffle, selected[0]))
    return false;
  if (fiver_to_hazel != nullptr) {
    *fiver_to_hazel = std::dynamic_pointer_cast<Fiver>(selected[0]);
    if (*fiver_to_hazel == nullptr)
      return false;
  }
  if (parameters != nullptr && fluffle->parameters != nullptr)
    *parameters = freeze(*fluffle->parameters);
  return true;
}

bool validate_fiver_merge(std::shared_ptr<Fluffle> fluffle,
                          const std::vector<std::shared_ptr<Owsla>> &selected,
                          std::vector<std::shared_ptr<Fiver>> *fivers) {
  if (selected.size() < 2)
    return false;
  if (fivers != nullptr)
    fivers->clear();
  for (auto &warren : selected) {
    if (warren == nullptr || warren->name() != "fiver" ||
        !eligible(fluffle, warren))
      return false;
    auto fiver = std::dynamic_pointer_cast<Fiver>(warren);
    if (fiver == nullptr)
      return false;
    if (fivers != nullptr)
      fivers->push_back(fiver);
  }
  return true;
}

bool hazel_recovery_conflicts(const HazelMergeRecovery &recovery,
                              const OwslaShard &action) {
  if (owsla_ranges_overlap(recovery.target, action))
    return true;
  for (auto &source : recovery.sources)
    if (owsla_ranges_overlap(source, action))
      return true;
  return false;
}

bool prepare_hazel_recoveries(std::shared_ptr<Fluffle> fluffle,
                              const OwslaShard &action, std::string *error) {
  std::vector<HazelMergeRecovery> hazel_merges;
  for (auto &recovery : fluffle->hazel_merges) {
    if (recovery.target.start == action.start &&
        recovery.target.end == action.end)
      continue;
    if (hazel_recovery_conflicts(recovery, action)) {
      if (!remove_hazel_merge_segments(fluffle->working, recovery, error))
        return false;
    } else {
      hazel_merges.push_back(recovery);
    }
  }
  fluffle->hazel_merges = hazel_merges;
  return true;
}

bool validate_hazel_action(
    std::shared_ptr<Fluffle> fluffle,
    const std::vector<std::shared_ptr<Owsla>> &selected,
    std::vector<std::shared_ptr<Hazel>> *hazels, std::string *destination,
    std::shared_ptr<std::map<std::string, std::string>> *parameters,
    std::string *error) {
  if (fluffle == nullptr || fluffle->working == nullptr || selected.size() < 2)
    return false;
  if (hazels != nullptr)
    hazels->clear();
  addr sequence_start = 0;
  addr sequence_end = 0;
  addr previous_end = 0;
  for (size_t i = 0; i < selected.size(); i++) {
    auto warren = selected[i];
    if (warren == nullptr || warren->name() != "hazel" ||
        !eligible(fluffle, warren))
      return false;
    auto hazel = std::dynamic_pointer_cast<Hazel>(warren);
    if (hazel == nullptr)
      return false;
    addr current_start;
    addr current_end;
    hazel->get_sequence(&current_start, &current_end);
    if (current_start < 0 || current_end < current_start ||
        (i > 0 && current_start <= previous_end))
      return false;
    if (i == 0)
      sequence_start = current_start;
    previous_end = current_end;
    sequence_end = current_end;
    if (hazels != nullptr)
      hazels->push_back(hazel);
  }
  if (destination != nullptr)
    *destination = fluffle->working->make_name(
        hazel_default_name(sequence_start, sequence_end));
  if (parameters != nullptr) {
    *parameters = std::make_shared<std::map<std::string, std::string>>();
    if (fluffle->parameters != nullptr)
      **parameters = *fluffle->parameters;
  }
  return prepare_hazel_recoveries(
      fluffle, OwslaShard(sequence_start, sequence_end, ""), error);
}

void merge_worker(std::shared_ptr<Fluffle> fluffle) {
  auto retire = [&fluffle]() {
    assert(fluffle->workers > 0);
    --fluffle->workers;
  };
  for (;;) {
    MergeAction action = MergeAction::none;
    std::vector<std::shared_ptr<Fiver>> fivers;
    std::vector<std::shared_ptr<Hazel>> hazels;
    std::vector<std::shared_ptr<Owsla>> selected;
    std::shared_ptr<Fiver> fiver_to_hazel;
    std::shared_ptr<Owsla> start_warren;
    std::shared_ptr<Owsla> end_warren;
    std::string fiver_hazel_parameters;
    std::string hazel_merge_destination;
    std::shared_ptr<std::map<std::string, std::string>> hazel_parameters;
    std::string action_error;
    {
      std::lock_guard<std::mutex> _(fluffle->lock);
      bool cleanup = false;
      for (auto &warren : fluffle->warrens)
        if (warren == nullptr || warren->name() == "remove") {
          cleanup = true;
          break;
        }
      if (cleanup) {
        std::vector<std::shared_ptr<Owsla>> warrens;
        for (auto &warren : fluffle->warrens)
          if (warren != nullptr && warren->name() != "remove")
            warrens.push_back(warren);
        fluffle->warrens = warrens;
      }
      size_t start;
      size_t end;
      if (!find_merge_action(fluffle, &start, &end)) {
        retire();
        return;
      }
      if (start > end || end >= fluffle->warrens.size()) {
        retire();
        return;
      }
      bool bad = false;
      for (size_t i = start; i <= end; i++) {
        auto warren = fluffle->warrens[i];
        if (warren == nullptr ||
            fluffle->merging.find(warren) != fluffle->merging.end()) {
          bad = true;
          break;
        }
        selected.push_back(warren);
      }
      if (bad || selected.size() == 0) {
        retire();
        return;
      }

      if (validate_fiver_conversion(fluffle, selected, &fiver_to_hazel,
                                    &fiver_hazel_parameters)) {
        action = MergeAction::fiver_to_hazel;
      } else if (validate_fiver_merge(fluffle, selected, &fivers)) {
        action = MergeAction::fiver_merge;
      } else if (validate_hazel_action(fluffle, selected, &hazels,
                                       &hazel_merge_destination,
                                       &hazel_parameters, &action_error)) {
        action = MergeAction::hazel_merge;
      }

      if (action == MergeAction::none) {
        retire();
        return;
      }
      for (auto &warren : selected)
        fluffle->merging.insert(warren);
      if (fluffle->workers < fluffle->max_workers) {
        fluffle->workers++;
        std::thread t(merge_worker, fluffle);
        t.detach();
      }
      start_warren = fluffle->warrens[start];
      end_warren = fluffle->warrens[end];
    }
    std::shared_ptr<Owsla> output;
    if (action == MergeAction::fiver_merge) {
      std::shared_ptr<Fiver> merged = Fiver::merge(fivers);
      if (merged != nullptr) {
        merged->pickle();
        output = merged;
      }
    } else if (action == MergeAction::hazel_merge) {
      std::string error;
      output = Hazel::merge(hazels, hazel_merge_destination, hazel_parameters,
                            &error);
    } else if (action == MergeAction::fiver_to_hazel) {
      std::string error;
      output = fiver_to_hazel->hazel(&error, 64 * 1024, fiver_hazel_parameters);
    }
    {
      std::lock_guard<std::mutex> _(fluffle->lock);
      if (output == nullptr) {
        for (auto &warren : selected)
          fluffle->merging.erase(warren);
        retire();
        return;
      }
      size_t start = 0;
      while (start < fluffle->warrens.size() &&
             fluffle->warrens[start] != start_warren)
        start++;
      size_t end = start;
      while (end < fluffle->warrens.size() &&
             fluffle->warrens[end] != end_warren)
        end++;
      if (start >= fluffle->warrens.size() ||
          end >= fluffle->warrens.size()) {
        assert(false);
        output->discard();
        for (auto &warren : selected)
          fluffle->merging.erase(warren);
        retire();
        return;
      }
      std::vector<std::shared_ptr<Owsla>> warrens;
      size_t i = 0;
      for (; i < start; i++)
        warrens.push_back(fluffle->warrens[i]);
      output->start();
      warrens.push_back(output);
      for (; i < end; i++)
        fluffle->merging.erase(fluffle->warrens[i]);
      fluffle->merging.erase(fluffle->warrens[i]);
      for (i++; i < fluffle->warrens.size(); i++)
        warrens.push_back(fluffle->warrens[i]);
      fluffle->warrens = warrens;
      if (fluffle->warrens.size() == 1)
        fluffle->cache = std::make_shared<OwslaCache>();
    }
    for (auto &warren : selected)
      warren->discard();
  }
}
} // namespace

void Bigwig::try_merge() {
  std::lock_guard<std::mutex> _(fluffle_->lock);
  if (fluffle_parameter_enabled(fluffle_, "merge") &&
      fluffle_->workers < fluffle_->max_workers) {
    fluffle_->workers++;
    std::thread t(merge_worker, fluffle_);
    t.detach();
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

bool Bigwig::commit_all(std::vector<std::shared_ptr<Bigwig>> bigwigs) {
  if (bigwigs.size() == 0)
    return true;
  std::shared_ptr<Working> working = bigwigs[0]->working();
  if (working == nullptr)
    return false;
  for (auto &bigwig : bigwigs)
    if (bigwig == nullptr || bigwig->working() != working)
      return false;
  std::string temp_name = working->make_temp("commit");
  std::ofstream script(temp_name);
  if (script.fail())
    return false;
  script << "#!/bin/sh\n";
  for (auto &bigwig : bigwigs) {
    assert(bigwig->fiver_ != nullptr);
    std::string command =
        bigwig->fiver_ == nullptr ? "" : bigwig->fiver_->commit_command();
    script << command;
    if (command.size() == 0 || command.back() != '\n')
      script << "\n";
  }
  script.close();
  if (script.fail()) {
    std::remove(temp_name.c_str());
    return false;
  }
  if (chmod(temp_name.c_str(), 0700) != 0) {
    std::remove(temp_name.c_str());
    return false;
  }
  std::ostringstream name;
  name << "commit." << bigwigs[0].get() << ".sh";
  std::string commit_name = name.str();
  std::string commit_path = working->make_name(commit_name);
  if (link(temp_name.c_str(), commit_path.c_str()) != 0) {
    std::remove(temp_name.c_str());
    return false;
  }
  std::remove(temp_name.c_str());
  for (auto &bigwig : bigwigs)
    bigwig->commit();
  working->remove(commit_name);
  return true;
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
