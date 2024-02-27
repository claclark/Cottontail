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
    if (started_) {
      lock_.unlock();
      safe_set(error) = "Commitable does not support concurrent transactions.";
      return false;
    }
    started_ = transaction_(error);
    bool result = started_;
    lock_.unlock();
    return result;
  }
  inline bool ready() {
    lock_.lock();
    assert(started_);
    bool result;
    if (readied_) {
      result = vote_;
    } else {
      readied_ = true;
      vote_ = ready_();
      result = vote_;
    }
    lock_.unlock();
    return result;
  }
  inline void commit() {
    lock_.lock();
    assert(started_ && vote_);
    commit_();
    started_ = readied_ = vote_ = false;
    lock_.unlock();
  }
  inline void abort() {
    lock_.lock();
    assert(started_);
    abort_();
    started_ = readied_ = vote_ = false;
    lock_.unlock();
  }
  inline bool transacting() {
    return started_;
  }

protected:
  Committable(){};

private:
  std::mutex lock_;
  bool started_ = false;
  bool readied_ = false;
  bool vote_ = false;
  virtual bool transaction_(std::string *error = nullptr) {
    safe_set(error) = "Committable does not support transactions";
    return false;
  };
  virtual bool ready_() { return false; };
  virtual void commit_() { return; };
  virtual void abort_() { return; };
};

} // namespace cottontail

#endif // COTTONTAIL_SRC_COMMITTABLE_H_
