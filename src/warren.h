#ifndef COTTONTAIL_SRC_WARREN_H_
#define COTTONTAIL_SRC_WARREN_H_

#include <cassert>
#include <memory>
#include <string>

#include "src/core.h"
#include "src/featurizer.h"
#include "src/gcl.h"
#include "src/hopper.h"
#include "src/idx.h"
#include "src/parse.h"
#include "src/stemmer.h"
#include "src/tokenizer.h"
#include "src/txt.h"
#include "src/working.h"

namespace cottontail {

class Stats;

class Warren {
public:
  static std::shared_ptr<Warren> make(const std::string &name,
                                      const std::string &burrow,
                                      std::string *error = nullptr);
  static std::shared_ptr<Warren> make(const std::string &name,
                                      std::string *error = nullptr) {
    std::string empty = "";
    return make(name, empty, error);
  }
  Warren(std::shared_ptr<Working> working,
         std::shared_ptr<Featurizer> featurizer,
         std::shared_ptr<Tokenizer> tokenizer, std::shared_ptr<Idx> idx,
         std::shared_ptr<Txt> txt)
      : default_container_(std::string("")),
        stemmer_(std::make_shared<NullStemmer>()), working_(working),
        featurizer_(featurizer), tokenizer_(tokenizer), idx_(idx), txt_(txt){};
  inline std::string name() { return name_; }
  inline void start() {
    // Aspirational stub for update synchronization
    assert(!started_);
    started_ = true;
  };
  inline void end() {
    assert(started_);
    started_ = false;
    // Aspirational stub for update synchronization
  }
  inline void started() { assert(started_); }
  inline std::shared_ptr<Working> working() {
    assert(started_);
    return working_;
  };
  inline std::shared_ptr<Featurizer> featurizer() {
    assert(started_);
    return featurizer_;
  };
  inline std::shared_ptr<Tokenizer> tokenizer() {
    assert(started_);
    return tokenizer_;
  };
  inline std::shared_ptr<Idx> idx() {
    assert(started_);
    return idx_;
  };
  inline std::shared_ptr<Txt> txt() {
    assert(started_);
    return txt_;
  };
  inline std::shared_ptr<Stats> stats() {
    assert(started_);
    return stats_;
  }
  inline std::string default_container() {
    assert(started_);
    return default_container_;
  };
  bool set_default_container(const std::string &container,
                             std::string *error = nullptr) {
    default_container_ = container;
    std::string container_key = "container";
    return set_parameter(container_key, container, error);
  };
  inline std::shared_ptr<Stemmer> stemmer() {
    assert(started_);
    return stemmer_;
  }
  bool set_stemmer(std::shared_ptr<Stemmer> stemmer,
                   std::string *error = nullptr) {
    stemmer_ = stemmer;
    std::string stemmer_key = "stemmer";
    std::string stemmer_value = stemmer->name();
    return set_parameter(stemmer_key, stemmer_value, error);
  }
  inline std::unique_ptr<Hopper> hopper_from_gcl(const std::string &gcl,
                                                 std::string *error = nullptr) {
    std::shared_ptr<gcl::SExpression> expr =
        cottontail::gcl::SExpression::from_string(gcl, error);
    if (expr == nullptr)
      return nullptr;
    expr = expr->expand_phrases(tokenizer());
    std::unique_ptr<Hopper> hopper = expr->to_hopper(featurizer(), idx());
    if (hopper == nullptr)
      safe_set(error) = "Could not construct hopper from valid gcl: " + gcl;
    return hopper;
  }
  inline bool set_parameter(const std::string &key, const std::string &value,
                            std::string *error = nullptr) {
    return set_parameter_(key, value, error);
  }
  inline bool get_parameter(const std::string &key, std::string *value,
                            std::string *error = nullptr) {
    return get_parameter_(key, value, error);
  }

  virtual ~Warren(){};
  Warren(const Warren &) = delete;
  Warren &operator=(const Warren &) = delete;
  Warren(Warren &&) = delete;
  Warren &operator=(Warren &&) = delete;

protected:
  std::string default_container_;
  std::shared_ptr<Stemmer> stemmer_ = nullptr;

private:
  virtual bool set_parameter_(const std::string &key, const std::string &value,
                              std::string *error) = 0;
  virtual bool get_parameter_(const std::string &key, std::string *value,
                              std::string *error) = 0;
  std::string name_ = "";
  bool started_ = false;
  std::shared_ptr<Working> working_ = nullptr;
  std::shared_ptr<Featurizer> featurizer_ = nullptr;
  std::shared_ptr<Tokenizer> tokenizer_ = nullptr;
  std::shared_ptr<Idx> idx_ = nullptr;
  std::shared_ptr<Txt> txt_ = nullptr;
  std::shared_ptr<Stats> stats_ = nullptr;
};
} // namespace cottontail

#endif // COTTONTAIL_SRC_WARREN_H_
