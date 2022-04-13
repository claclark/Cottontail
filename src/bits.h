#ifndef COTTONTAIL_SRC_BITS_H_
#define COTTONTAIL_SRC_BITS_H_

#include "src/core.h"

namespace cottontail {

class BitsIn final {
public:
  BitsIn(void *buffer)
      : start_((unsigned char *)buffer), next_((unsigned char *)buffer){};

  inline void push(unsigned char bit) {
    if (bit)
      current_ |= mask_;
    if ((++count_) % 8 == 0) {
      *next_++ = current_;
      mask_ = 1;
      current_ = 0;
    } else {
      mask_ <<= 1;
    }
  };

  inline void *bits() {
    *next_ = current_;
    return (void *)start_;
  }

  inline addr count() { return count_; }

  BitsIn(const BitsIn &) = delete;
  BitsIn &operator=(const BitsIn &) = delete;
  BitsIn(BitsIn &&) = delete;
  BitsIn &operator=(BitsIn &&) = delete;

protected:
  BitsIn(){};

private:
  addr count_ = 0;
  unsigned char mask_ = 1;
  unsigned char current_ = 0;
  unsigned char *start_;
  unsigned char *next_;
};

class BitsOut final {
public:
  BitsOut(void *buffer, addr count)
      : count_(count), next_((unsigned char *)buffer) {
    if (count_ > 0)
      current_ = *next_++;
    else
      count_ = 0;
  };

  inline unsigned char front() { return (unsigned char)(current_ & 1); }

  inline bool empty() { return front_ >= count_; }

  inline void pop() {
    front_++;
    if (empty()) {
      current_ = 0;
    } else {
      if (front_ % 8 == 0)
        current_ = *next_++;
      else
        current_ >>= 1;
    }
  }

private:
  addr count_;
  addr front_ = 0;
  unsigned char *next_;
  unsigned char current_ = 0;
};

} // namespace cottontail
#endif // COTTONTAIL_SRC_BITS_H_
