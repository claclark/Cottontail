#ifndef COTTONTAIL_SRC_BUILDER_H_
#define COTTONTAIL_SRC_BUILDER_H_

#include <fstream>
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "src/core.h"
#include "src/featurizer.h"
#include "src/tokenizer.h"
#include "src/working.h"

namespace cottontail {

class Builder {
public:
  bool add_text(const std::string &text, addr *p, addr *q,
                std::string *error = nullptr) {
    if (failed_) {
      safe_set(error) = "Can't add text after build failed";
      return false;
    } else if (finalized_) {
      safe_set(error) = "Can't add text after build finalized";
      return false;
    } else {
      failed_ = !add_text_(text, p, q, error);
      return !failed_;
    }
  };
  bool add_text(const std::string &text, std::string *error = nullptr) {
    addr p, q;
    return add_text(text, &p, &q, error);
  };
  bool add_annotation(const std::string &tag, addr p, addr q, fval v,
                      std::string *error = nullptr) {
    if (failed_) {
      safe_set(error) = "Can't add annotations after build failed";
      return false;
    } else if (finalized_) {
      safe_set(error) = "Can't add annotations after build finalized";
      return false;
    } else {
      failed_ = !add_annotation_(tag, p, q, v, error);
      return !failed_;
    }
  }
  bool add_annotation(const std::string &tag, addr p, addr q, addr v,
                      std::string *error = nullptr) {
    return(add_annotation(tag, p, q, addr2fval(v), error));
  };
  bool finalize(std::string *error = nullptr) {
    if (failed_) {
      safe_set(error) = "Can't finalize a failed build";
      return false;
    } else if (finalized_) {
      return true;
    } else {
      finalized_ = true;
      failed_ = !finalize_(error);
      return !failed_;
    }
  }
  bool finalized() { return finalized_; }
  bool verbose(bool setting) {
    bool old = verbose_;
    verbose_ = setting;
    return old;
  };
  bool verbose() { return verbose_; }

  virtual ~Builder(){};
  Builder(const Builder &) = delete;
  Builder &operator=(const Builder &) = delete;
  Builder(Builder &&) = delete;
  Builder &operator=(Builder &&) = delete;

protected:
  Builder(){};

private:
  virtual bool add_text_(const std::string &text, addr *p, addr *q,
                         std::string *error) = 0;
  virtual bool add_annotation_(const std::string &tag, addr p, addr q, fval v,
                               std::string *error) = 0;
  virtual bool finalize_(std::string *error) = 0;
  bool finalized_ = false;
  bool failed_ = false;
  bool verbose_ = false;
};

bool build_trec(const std::vector<std::string> &text,
                std::shared_ptr<Builder> builder, std::string *error = nullptr);
bool build_json(const std::vector<std::string> &text,
                std::shared_ptr<Builder> builder, std::string *error = nullptr);
std::shared_ptr<std::string> inhale(const std::string &filename,
                                    std::string *error = nullptr);
} // namespace cottontail
#endif // COTTONTAIL_SRC_BUILDER_H_
