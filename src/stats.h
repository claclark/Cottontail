#ifndef COTTONTAIL_SRC_STATS_H_
#define COTTONTAIL_SRC_STATS_H_

#include <memory>
#include <string>

#include "src/core.h"
#include "src/hopper.h"
#include "src/warren.h"

namespace cottontail {
class Stats {
public:
  static std::shared_ptr<Stats> make(const std::string &name,
                                     const std::string &recipe,
                                     std::shared_ptr<Warren> warren,
                                     std::string *error = nullptr);
  static std::shared_ptr<Stats> make( std::shared_ptr<Warren> warren,
                                     std::string *error = nullptr);
  static bool check(const std::string &name, const std::string &recipe,
                    std::string *error = nullptr);
  inline std::string recipe() { return recipe_(); }
  inline std::string name() { return name_; }

  inline bool have(const std::string &name) { return have_(name); };
  inline addr df(const std::string &term) { return idf_(term); };
  inline fval idf(const std::string &term) { return idf_(term); };
  inline fval rsj(const std::string &term) { return rsj_(term); };
  inline fval avgl() { return avgl_(); };
  inline std::unique_ptr<Hopper> tf_hopper(const std::string &term) {
    return tf_hopper_(term);
  };

  virtual ~Stats(){};
  Stats(const Stats &) = delete;
  Stats &operator=(const Stats &) = delete;
  Stats(Stats &&) = delete;
  Stats &operator=(Stats &&) = delete;

protected:
  Stats() = delete;
  Stats(std::shared_ptr<Warren> warren) : warren_(warren){};
  std::shared_ptr<Warren> warren_;

private:
  virtual std::string recipe_() { return ""; }
  virtual bool have_(const std::string &name) { return false; };
  virtual fval avgl_() { return 1; };
  virtual fval idf_(const std::string &term) { return 0.0; }
  virtual fval rsj_(const std::string &term) { return 0.0; }
  virtual std::unique_ptr<Hopper> tf_hopper_(const std::string &term) {
    return std::make_unique<EmptyHopper>();
  }
  std::string name_ = "";
};
} // namespace cottontail
#endif // COTTONTAIL_SRC_STATS_H_
