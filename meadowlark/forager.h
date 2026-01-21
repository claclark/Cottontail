#ifndef COTTONTAIL_MEADOWLARK_FORAGER_H_
#define COTTONTAIL_MEADOWLARK_FORAGER_H_

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "src/annotator.h"
#include "src/committable.h"
#include "src/core.h"
#include "src/hopper.h"
#include "src/warren.h"

namespace cottontail {
namespace meadowlark {

class Forager : public Committable {
public:
  static std::shared_ptr<Forager>
  make(std::shared_ptr<Warren> warren, const std::string &name,
       const std::string &tag,
       const std::map<std::string, std::string> &parameters,
       std::string *error = nullptr);
  static std::shared_ptr<Forager> make(std::shared_ptr<Warren> warren,
                                       const std::string &name,
                                       const std::string &tag,
                                       std::string *error = nullptr) {
    std::map<std::string, std::string> parameters;
    return make(warren, name, tag, parameters, error);
  };
  static bool check(const std::string &name, const std::string &tag,
                    const std::map<std::string, std::string> &parameters,
                    std::string *error = nullptr);
  static bool check(const std::string &name, const std::string &tag,
                    std::string *error = nullptr) {
    std::map<std::string, std::string> parameters;
    return check(name, tag, parameters, error);
  };
  bool label(std::string *error = nullptr);
  inline bool forage(addr p, addr q, std::string *error = nullptr) {
    std::lock_guard<std::mutex> _(mutex_);
    return forage_(p, q, error);
  };
  inline bool forage(const std::vector<std::pair<addr, addr>> &intervals,
                     size_t start, size_t n, std::string *error = nullptr) {
    std::lock_guard<std::mutex> _(mutex_);
    for (size_t i = start; i < start + n; i++)
      if (!forage_(intervals[i].first, intervals[i].second, error))
        return false;
    return true;
  };
  inline bool forage(std::shared_ptr<Hopper> hopper, addr start, addr end,
                     std::string *error = nullptr) {
    std::lock_guard<std::mutex> _(mutex_);
    addr p, q;
    for (hopper->tau(start, &p, &q); q <= end; hopper->tau(p + 1, &p, &q))
      if (!forage_(p, q, error))
        return false;
    return true;
  };
  inline bool forage(std::shared_ptr<Hopper> hopper,
                     std::string *error = nullptr) {
    return forage(hopper, minfinity + 1, maxfinity - 1, error);
  };

  virtual ~Forager(){};
  Forager(const Forager &) = delete;
  Forager &operator=(const Forager &) = delete;
  Forager(Forager &&) = delete;
  Forager &operator=(Forager &&) = delete;

protected:
  Forager(){};
  std::mutex mutex_;
  std::string name_, tag_;
  std::map<std::string, std::string> parameters_;
  std::shared_ptr<Warren> warren_;
private:
  virtual bool forage_(addr p, addr q, std::string *error) = 0;
  bool transaction_(std::string *error) { return warren_->transaction(error); };
  bool ready_() { return warren_->ready(); };
  void commit_() { return warren_->commit(); }
  void abort_() { return warren_->abort(); }
};

std::string forager_label(const std::string &name, const std::string &tag);
std::string forager2json(const std::string &name, const std::string &tag,
                         const std::map<std::string, std::string> &parameters);
bool json2forager(const std::string &json, std::string *name, std::string *tag,
                  std::map<std::string, std::string> *parameters,
                  std::string *error = nullptr);

} // namespace meadowlark
} // namespace cottontail

#endif // COTTONTAIL_MEADOWLARK_FORAGER_H_
