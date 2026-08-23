#include "meadowlark/tf-idf_stats.h"

#include <cassert>
#include <cmath>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "meadowlark/forager.h"
#include "meadowlark/metadata.h"
#include "src/core.h"
#include "src/hopper.h"
#include "src/tagging_featurizer.h"
#include "src/tf_hopper.h"
#include "src/warren.h"

namespace cottontail {
namespace meadowlark {

namespace {
std::string gcl_string(const std::string &s) {
  std::string quoted = "\"";
  for (char c : s) {
    if (c == '\\' || c == '"')
      quoted += '\\';
    quoted += c;
  }
  quoted += '"';
  return quoted;
}

std::string forager_query(const std::string &name, const std::string &tag) {
  std::string typed = "(>> @ (>> :type: \"forager\"))";
  std::string named =
      "(>> " + typed + " (>> :name: " + gcl_string(name) + "))";
  return "(>> " + named + " (>> :tag: " + gcl_string(tag) + "))";
}

std::string current_forager_query(const std::string &name,
                                  const std::string &tag) {
  return "(>> " + forager_query(name, tag) + " :query:)";
}

void no_stats(const std::string &recipe, std::string *error) {
  if (recipe == "")
    safe_error(error) = "No tf-idf stats in meadow";
  else
    safe_error(error) = "No tf-idf stats in meadow for tag: " + recipe;
}
} // namespace

std::shared_ptr<Stats> TfIdfStats::make(const std::string &recipe,
                                        std::shared_ptr<Warren> warren,
                                        std::string *error) {
  addr p, q;
  std::string metadata_tag = recipe;
  std::unique_ptr<Hopper> hopper;
  bool legacy = false;
  ForagerMetadata metadata;
  bool have_metadata = false;
  if (recipe == "") {
    hopper =
        warren->idx()->hopper(warren->featurizer()->featurize("@tf-idf:"));
    if (hopper != nullptr) {
      hopper->tau(minfinity + 1, &p, &q);
      legacy = p < maxfinity;
    }
    if (!legacy)
      metadata_tag = "none";
  }
  if (!legacy) {
    std::vector<std::string> queries = {
        current_forager_query("tf-idf", metadata_tag),
        forager_query("tf-idf", metadata_tag)};
    for (const auto &query : queries) {
      hopper = warren->hopper_from_gcl(query, error);
      if (hopper == nullptr)
        return nullptr;
      for (hopper->tau(minfinity + 1, &p, &q); p < maxfinity;
           hopper->tau(p + 1, &p, &q)) {
        ForagerMetadata candidate;
        if (!json2forager(warren->txt()->translate(p, q), &candidate, error))
          return nullptr;
        if (candidate.has_filename)
          continue;
        metadata = candidate;
        have_metadata = true;
        break;
      }
      if (have_metadata)
        break;
    }
    if (!have_metadata) {
      no_stats(recipe, error);
      return nullptr;
    }
  }
  if (legacy &&
      !json2forager(warren->txt()->translate(p, q), &metadata, error))
    return nullptr;
  if (metadata.name != "tf-idf" || metadata.tag != metadata_tag) {
    safe_error(error) = "Metadata inconsistency";
    return nullptr;
  }
  std::map<std::string, std::string> parameters = metadata.parameters;
  std::string label = forager_label("tf-idf", metadata_tag);
  std::string id_query;
  if (parameters.find("id") != parameters.end())
    id_query = parameters["id"];
  std::string container_query;
  if (parameters.find("container") == parameters.end())
    container_query = ":";
  else
    container_query = parameters["container"];
  if ((hopper = warren->hopper_from_gcl(container_query, error)) == nullptr)
    return nullptr;
  std::string contents_query;
  if (metadata.has_query)
    contents_query = metadata.query;
  else if (parameters.find("contents") != parameters.end())
    contents_query = parameters["contents"];
  else if (parameters.find("gcl") != parameters.end())
    contents_query = parameters["gcl"];
  else
    contents_query = container_query;
  if ((hopper = warren->hopper_from_gcl(contents_query, error)) == nullptr)
    return nullptr;
  std::string stemmer_name;
  if (parameters.find("stemmer") == parameters.end())
    stemmer_name = "porter";
  else
    stemmer_name = parameters["stemmer"];
#if 0
  // Transitional artifact from the older Warren-global parameter model.
  // TfIdfStats now owns the ranking-view stemmer directly; remove this block
  // once its clear that its not required.
  if (!warren->set_parameter("stemmer", stemmer_name, error))
    return nullptr;
#endif
  std::shared_ptr<Stemmer> stemmer = Stemmer::make(stemmer_name, "", error);
  if (stemmer == nullptr)
    return nullptr;
  std::shared_ptr<Tokenizer> tokenizer;
  if (parameters.find("tokenizer") == parameters.end())
    tokenizer = Tokenizer::make("ascii", "", error);
  else
    tokenizer = Tokenizer::make(parameters["tokenizer"], "", error);
  if (tokenizer == nullptr)
    return nullptr;
  std::shared_ptr<TfIdfStats> stats =
      std::shared_ptr<TfIdfStats>(new TfIdfStats(warren, stemmer, tokenizer));
  stats->tag_ = metadata_tag;
  stats->label_ = label + ":";
  stats->id_query_ = id_query;
  stats->contents_query_ = contents_query;
  stats->container_query_ = container_query;
  stats->tf_featurizer_ =
      TaggingFeaturizer::make(warren->featurizer(), label + "tf", error);
  if (stats->tf_featurizer_ == nullptr)
    return nullptr;
  std::shared_ptr<Featurizer> total_featurizer =
      TaggingFeaturizer::make(warren->featurizer(), label + "total", error);
  hopper = warren->idx()->hopper(total_featurizer->featurize("items"));
  if (hopper == nullptr) {
    no_stats(recipe, error);
    return nullptr;
  }
  addr n, items = 0;
  for (hopper->tau(minfinity + 1, &p, &q, &n); p < maxfinity;
       hopper->tau(p + 1, &p, &q, &n))
    items += n;
  if (items < 1) {
    no_stats(recipe, error);
    return nullptr;
  }
  stats->items_ = items;
  hopper = warren->idx()->hopper(total_featurizer->featurize("length"));
  if (hopper == nullptr) {
    no_stats(recipe, error);
    return nullptr;
  }
  addr length = 0;
  for (hopper->tau(minfinity + 1, &p, &q, &n); p < maxfinity;
       hopper->tau(p + 1, &p, &q, &n))
    length += n;
  if (length < 1) {
    no_stats(recipe, error);
    return nullptr;
  }
  stats->average_length_ = length / stats->items_;
  stats->tf_featurizer_ =
      TaggingFeaturizer::make(warren->featurizer(), label + "tf", error);
  if (stats->tf_featurizer_ == nullptr)
    return nullptr;
  return stats;
}

bool TfIdfStats::check(const std::string &recipe, std::string *error) {
  return true;
}

std::string TfIdfStats::recipe_() { return tag_; }

bool TfIdfStats::have_(const std::string &name) {
  return name == "content" || name == "avgl" || name == "rsj" ||
         name == "idf" || name == "tf";
}

fval TfIdfStats::avgl_() { return average_length_; }

namespace {
inline addr load_df(std::shared_ptr<Warren> warren,
                    std::shared_ptr<Featurizer> featurizer,
                    const std::string &term) {
  return warren->idx()->count(featurizer->featurize(term));
}
} // namespace

fval TfIdfStats::idf_(const std::string &term) {
  fval df = (fval)load_df(warren_, tf_featurizer_, term);
  if (df == 0.0)
    return 0.0;
  fval idf = std::log(items_ / df);
  if (idf < 0.0)
    return 0.0;
  return idf;
}

fval TfIdfStats::rsj_(const std::string &term) {
  fval df = (fval)load_df(warren_, tf_featurizer_, term);
  if (df == 0.0)
    return 0.0;
  fval rsj = std::log((items_ - df + 0.5) / (df + 0.5));
  if (rsj < 0.0)
    return 0.0;
  return rsj;
}

std::unique_ptr<Hopper> TfIdfStats::tf_hopper_(const std::string &term) {
  std::unique_ptr<Hopper> tf_hopper =
      warren_->idx()->hopper(tf_featurizer_->featurize(term));
  assert(tf_hopper != nullptr);
  std::unique_ptr<Hopper> chopper = warren_->hopper_from_gcl(contents_query_);
  assert(chopper != nullptr);
  return std::make_unique<TfHopper>(std::move(tf_hopper), std::move(chopper));
}

std::unique_ptr<Hopper> TfIdfStats::container_hopper_() {
  std::unique_ptr<Hopper> hopper = warren_->hopper_from_gcl(container_query_);
  assert(hopper != nullptr);
  return hopper;
}

std::unique_ptr<Hopper> TfIdfStats::id_hopper_() {
  if (id_query_ == "")
    return nullptr;
  std::unique_ptr<Hopper> hopper = warren_->hopper_from_gcl(id_query_);
  return hopper;
}
} // namespace meadowlark
} // namespace cottontail
