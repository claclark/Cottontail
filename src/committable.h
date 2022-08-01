#ifndef COTTONTAIL_SRC_COMMITTABLE_H_
#define COTTONTAIL_SRC_COMMITTABLE_H_

#include <cassert>
#include <mutex>
#include <string>

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
    lock_.lock();
    assert(!started_ && !vote_);
    started_ = transaction_(error);
    bool result = started_;
    lock_.unlock();
    return result;
  }
  inline bool ready() {
    lock_.lock();
    assert(started_);
    vote_ = ready_();
    bool result = vote_;
    lock_.unlock();
    return result;
  }
  inline void commit() {
    lock_.lock();
    assert(started_ && vote_);
    commit_();
    started_ = vote_ = false;
    lock_.unlock();
  }
  inline void abort() {
    lock_.lock();
    assert(started_);
    abort_();
    started_ = vote_ = false;
    lock_.unlock();
  }

protected:
  Committable(){};

private:
  std::mutex lock_;
  bool started_ = false;
  bool vote_ = false;
  virtual bool transaction_(std::string *error = nullptr) {
    safe_set(error) = "Committable does not support transactions";
    return false;
  };
  virtual bool ready_() { return false; };
  virtual void commit_() { return; };
  virtual void abort_() { return; }
};

} // namespace cottontail

#endif // COTTONTAIL_SRC_COMMITTABLE_H_
