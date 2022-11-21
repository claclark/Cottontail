#ifndef COTTONTAIL_TEST_FAKE_TEXT_H_
#define COTTONTAIL_TEST_FAKE_TEXT_H_

#include <memory>
#include <string>

class FakeText {
public:
  FakeText(){};
  ~FakeText(){};
  FakeText(const FakeText &) = delete;
  FakeText &operator=(const FakeText &) = delete;
  FakeText(FakeText &&) = delete;
  FakeText &operator=(FakeText &&) = delete;

  static std::string generate(size_t start, size_t end) {
    if (start < 0)
      start = 0;
    std::string text = "";
    for (size_t n = start; n <= end; n++) {
      if (n > start)
        text += "\n";
      text += std::to_string(n);
    }
    return text;
  };

  static bool check(std::shared_ptr<cottontail::Warren> warren, size_t u,
                    size_t v) {
    std::string t = warren->txt()->translate(u, v);
    std::string f = FakeText::generate(u, v);
    f += "\n";
    return t == f;
  };

  std::string text(size_t tokens) {
    std::string text;
    if (tokens <= 0) {
      text = "";
    } else {
      text = generate(next_, next_ + (tokens - 1));
      next_ += tokens;
    }
    return text;
  };

private:
  size_t next_ = 0;
};

#endif // COTTONTAIL_TEST_FAKE_TEXT_H_
