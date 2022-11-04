#include "src/fiver.h"

#include <iostream>
#include <map>

#include "src/annotator.h"
#include "src/appender.h"
#include "src/core.h"
#include "src/fastid_txt.h"
#include "src/featurizer.h"
#include "src/recipe.h"
#include "src/simple.h"
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

std::shared_ptr<Fiver> Fiver::make(
    std::shared_ptr<Working> working, std::shared_ptr<Featurizer> featurizer,
    std::shared_ptr<Tokenizer> tokenizer, addr identifier, std::string *error,
    std::shared_ptr<std::map<std::string, std::string>> parameters) {
  return nullptr;
}
} // namespace cottontail
