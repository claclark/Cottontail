#ifndef COTTONTAIL_MEADOWLARK_FORAGER_H_
#define COTTONTAIL_MEADOWLARK_FORAGER_H_

#include <map>
#include <memory>
#include <string>

#include "src/core.h"
#include "src/warren.h"

namespace cottontail {
namespace meadowlark {

class Forager {
public:
  static std::shared_ptr<Forager>
  make(std::shared_ptr<Warren> warren, const std::string &name,
       const std::string &tag,
       const std::map<std::string, std::string> &parameters,
       std::string *error = nullptr);
  static std::shared_ptr<Forager> make(std::shared_ptr<Warren> warren,
                                       const std::string &name,
                                       const std::string &recipe,
                                       std::string *error = nullptr);
  static bool check(std::shared_ptr<Warren> warren,
                    const std::string &query, const std::string &name,
                    const std::string &tag,
                    const std::map<std::string, std::string> &parameters,
                    std::string *error = nullptr);
  inline bool forage(addr p, addr q, std::string *error = nullptr) {
    return forage_(p, q, error);
  };
  inline bool finish(std::string *error = nullptr) { return finish_(error); };

  virtual ~Forager(){};
  Forager(const Forager &) = delete;
  Forager &operator=(const Forager &) = delete;
  Forager(Forager &&) = delete;
  Forager &operator=(Forager &&) = delete;

protected:
  Forager(){};
  std::shared_ptr<Warren> warren_;

private:
  virtual bool forage_(addr p, addr q, std::string *error) = 0;
  virtual bool finish_(std::string *error) { return true; };
};

std::string forager_label(const std::string &name, const std::string &tag);

} // namespace meadowlark
} // namespace cottontail

#endif // COTTONTAIL_MEADOWLARK_FORAGER_H_
