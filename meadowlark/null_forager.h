#ifndef COTTONTAIL_MEADOWLARK_NULL_FORAGER_H_
#define COTTONTAIL_MEADOWLARK_NULL_FORAGER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "meadowlark/forager.h"
#include "src/annotator.h"
#include "src/core.h"
#include "src/tokenizer.h"
#include "src/warren.h"

namespace cottontail {
namespace meadowlark {

class NullForager : public Forager {
public:
  static std::shared_ptr<Forager>
  make(std::shared_ptr<Warren> warren, const std::string &tag,
       const std::map<std::string, std::string> &parameters,
       std::string *error = nullptr) {
    return std::shared_ptr<Forager>(new NullForager());
  }
  virtual ~NullForager(){};
  NullForager(const NullForager &) = delete;
  NullForager &operator=(const NullForager &) = delete;
  NullForager(NullForager &&) = delete;
  NullForager &operator=(NullForager &&) = delete;

private:
  NullForager(){};
  bool forage_(addr p, addr q, std::string *error) final { return true; }
};
} // namespace meadowlark
} // namespace cottontail

#endif // COTTONTAIL_MEADOWLARK_NULL_FORAGER_H_
