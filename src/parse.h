#ifndef COTTONTAIL_SRC_PARSE_H_
#define COTTONTAIL_SRC_PARSE_H_

#include <memory>
#include <string>
#include <vector>

#include "src/core.h"
#include "src/featurizer.h"
#include "src/gcl.h"
#include "src/hopper.h"
#include "src/idx.h"
#include "src/tokenizer.h"

namespace cottontail {

namespace gcl {

enum Operator {
  TERM,
  FIXED,
  ONE_OF,
  ALL_OF,
  FOLLOWED_BY,
  CONTAINED_IN,
  CONTAINING,
  NOT_CONTAINED_IN,
  NOT_CONTAINING,
  LINK
};

class SExpression final {
public:
  static std::shared_ptr<SExpression> from_string(std::string s,
                                                  std::string *error);
  std::string to_string();
  std::shared_ptr<SExpression> to_binary();
  std::shared_ptr<SExpression>
  expand_phrases(std::shared_ptr<Tokenizer> tokenizer, char marker = '"');
  std::unique_ptr<Hopper> to_hopper(std::shared_ptr<Featurizer> featurizer,
                                    std::shared_ptr<Idx> idx);

  friend const char *parse_expr(const char *where,
                                std::shared_ptr<SExpression> expr, bool *okay);

private:
  Operator kind_;
  std::string term_;
  addr width_;
  std::vector<std::shared_ptr<SExpression>> subx_;
};
} // namespace gcl
} // namespace cottontail
#endif // COTTONTAIL_SRC_PARSE_H_
