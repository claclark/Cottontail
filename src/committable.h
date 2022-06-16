#ifndef COTTONTAIL_SRC_COMMITTABLE_H_
#define COTTONTAIL_SRC_COMMITTABLE_H_

#include <cassert>

namespace cottontail {

class Committable {
public:
  virtual ~Committable(){};
  Committable(const Committable &) = delete;
  Committable &operator=(const Committable &) = delete;
  Committable(Committable &&) = delete;
  Committable &operator=(Committable &&) = delete;

  inline void transaction() {
    assert(!started_ && !vote_);
    transaction_();
    started_ = true;
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
  virtual void transaction_(){};
  virtual bool ready_() { return true; };
  virtual void commit_() { return; };
  virtual void abort_() { return; }
};

} // namespace cottontail

#endif // COTTONTAIL_SRC_COMMITTABLE_H_
