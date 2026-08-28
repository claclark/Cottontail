#ifndef COTTONTAIL_GCL_PARSE_H_
#define COTTONTAIL_GCL_PARSE_H_

#include <memory>
#include <string>
#include <vector>

#include "src/core.h"
#include "src/featurizer.h"
#include "gcl/gcl.h"
#include "src/hopper.h"
#include "src/idx.h"
#include "src/tokenizer.h"

namespace cottontail {

namespace gcl {

class Optimizer;

enum Operator {
  TERM,
  QUOTE,
  FIXED,
  ONE_OF,
  ALL_OF,
  FOLLOWED_BY,
  CONTAINED_IN,
  CONTAINING,
  NOT_CONTAINED_IN,
  NOT_CONTAINING,
  LINK,
  MATERIALIZE,
  ERROR
};

class SExpression final {
public:
  SExpression() = default;
  SExpression(const SExpression &) = delete;
  SExpression &operator=(const SExpression &) = delete;
  SExpression(SExpression &&) = delete;
  SExpression &operator=(SExpression &&) = delete;
  ~SExpression() = default;

  static std::shared_ptr<SExpression>
  make(Operator kind, const std::string &term, addr width,
       const std::vector<std::shared_ptr<SExpression>> &subx);
  static std::shared_ptr<SExpression> from_string(std::string s,
                                                  std::string *error);
  static std::shared_ptr<SExpression> make_error(const std::string &message);
  bool is_error() const { return kind_ == ERROR; }
  const std::string &message() const { return message_; }
  std::string to_string();
  std::shared_ptr<SExpression> to_binary();
  std::shared_ptr<SExpression>
  expand_phrases(std::shared_ptr<Tokenizer> tokenizer, char marker = '"');
  std::unique_ptr<Hopper> to_hopper(std::shared_ptr<Featurizer> featurizer,
                                    std::shared_ptr<Idx> idx);

  friend const char *parse_expr(const char *where,
                                std::shared_ptr<SExpression> expr, bool *okay);

  // Optimizer is the trusted GCL tree-rewrite boundary. Its private helpers
  // inspect and rebuild SExpression internals without exposing tree mutation
  // through the public parser API.
  friend class Optimizer;

private:
  Operator kind_;
  std::string term_;
  std::string message_;
  addr width_;
  std::vector<std::shared_ptr<SExpression>> subx_;
};
} // namespace gcl
} // namespace cottontail
#endif // COTTONTAIL_GCL_PARSE_H_
