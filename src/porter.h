#ifndef COTTONTAIL_SRC_PORTER_H_
#define COTTONTAIL_SRC_PORTER_H_

#include <iostream>
#include <mutex>
#include <string>

#include <ctype.h>
#include <string.h>

#include "src/core.h"
#include "src/stemmer.h"

// see porter.cc for explanation

// from original mainline in porter.c
#define LETTER(ch) (isupper(ch) || islower(ch))

namespace cottontail {

class Porter final : public Stemmer {
public:
  Porter() {
    b = new char[HUGE_ENGLISH_WORD + 1];
    b[HUGE_ENGLISH_WORD] = '\0';
  }

  virtual ~Porter() final { delete[] b; }
  Porter(const Porter &) = default;
  Porter &operator=(const Porter &) = default;
  Porter(Porter &&) = delete;
  Porter &operator=(Porter &&) = delete;

private:
  std::string stem_(const std::string &word, bool *stemmed) final {
    if (word.size() < 3 || word.size() > HUGE_ENGLISH_WORD) {
      if (stemmed != nullptr)
        *stemmed = false;
      return word;
    }
    lock_.lock();
    char *p = b;
    for (const char *q = word.c_str(); *q; q++)
      if (LETTER(*q)) {
        *p++ = tolower(*q);
      } else {
        if (stemmed != nullptr)
          *stemmed = false;
        lock_.unlock();
        return word;
      }
    size_t n = porter(b, 0, word.size() - 1);
    if (stemmed != nullptr)
      *stemmed = true;
    std::string stem = "porter:" + std::string(b, b + n + 1);
    lock_.unlock();
    return stem;
  };
  std::string recipe_() final { return ""; };

  const size_t HUGE_ENGLISH_WORD = 32;
  char *b;      /* buffer for word to be stemmed */
  int k, k0, j; /* j is a general offset into the string */
  std::mutex lock_;
  int cons(int i);
  int m();
  int vowelinstem();
  int doublec(int j);
  int cvc(int i);
  int ends(char *s);
  void setto(char *s);
  void r(char *s);
  int porter(char *p, int i, int j);
  void step1ab();
  void step1c();
  void step2();
  void step3();
  void step4();
  void step5();
};
} // namespace cottontail
#endif // COTTONTAIL_SRC_PORTER_H_
