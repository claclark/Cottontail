#ifndef COTTONTAIL_SRC_IDX_H_
#define COTTONTAIL_SRC_IDX_H_

#include <memory>
#include <string>

#include "src/core.h"
#include "src/hopper.h"
#include "src/working.h"

namespace cottontail {

class Idx {
public:
  static std::shared_ptr<Idx> make(const std::string &name,
                                   const std::string &recipe,
                                   std::string *error = nullptr,
                                   std::shared_ptr<Working> working = nullptr);
  static bool check(const std::string &name, const std::string &recipe,
                    std::string *error = nullptr);
  inline std::string recipe() { return recipe_(); }
  inline std::string name() { return name_; }

  inline std::unique_ptr<Hopper> hopper(addr feature) {
    return hopper_(feature);
  };
  inline addr count(addr feature) { return count_(feature); };
  inline addr vocab() { return vocab_(); }
  inline void reset(){reset_();};

  virtual ~Idx(){};
  Idx(const Idx &) = delete;
  Idx &operator=(const Idx &) = delete;
  Idx(Idx &&) = delete;
  Idx &operator=(Idx &&) = delete;

protected:
  Idx(){};
  std::shared_ptr<Working> working_;

private:
  virtual std::string recipe_() = 0;
  virtual std::unique_ptr<Hopper> hopper_(addr feature) = 0;
  virtual addr count_(addr feature);
  virtual addr vocab_() = 0;
  virtual void reset_() = 0;
  std::string name_ = "";
};
} // namespace cottontail
#endif // COTTONTAIL_SRC_IDX_H_
