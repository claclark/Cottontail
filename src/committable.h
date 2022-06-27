#ifndef COTTONTAIL_SRC_COMMITTABLE_H_
#define COTTONTAIL_SRC_COMMITTABLE_H_

#include <cassert>

#include "src/core.h"

namespace cottontail {

class Committable {
public:
  virtual ~Committable(){};
  Committable(const Committable &) = delete;
  Committable &operator=(const Committable &) = delete;
  Committable(Committable &&) = delete;
  Committable &operator=(Committable &&) = delete;

  inline bool transaction(std::string *error = nullptr) {
    assert(!started_ && !vote_);
    started_ = transaction_(error);
    return started_;
  }
  inline bool ready() {
    assert(started_);
    vote_ = ready_();
    return vote_;
  }
  inline void commit() {
    assert(started_ && vote_);
    commit_();
    started_ = vote_ = false;
  }
  inline void abort() {
    assert(started_);
    abort_();
    started_ = vote_ = false;
  }

protected:
  Committable(){};

private:
  bool started_ = false;
  bool vote_ = false;
  virtual bool transaction_(std::string *error = nullptr) {
    safe_set(error) = "Committable does not support transactions";
    return false;
  };
  virtual bool ready_() { return true; };
  virtual void commit_() { return; };
  virtual void abort_() { return; }
};

} // namespace cottontail

#endif // COTTONTAIL_SRC_COMMITTABLE_H_
