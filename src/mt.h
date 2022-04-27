#ifndef COTTONTAIL_SRC_MT_H_
#define COTTONTAIL_SRC_MT_H_

#include <string>

namespace cottontail {

struct gcl_envStructure;

class Mt final {
public:
  Mt(){};
  Mt(const Mt &) = delete;
  Mt &operator=(const Mt &) = delete;
  Mt(Mt &&) = delete;
  Mt &operator=(Mt &&) = delete;
  ~Mt();
  bool infix_expression(const std::string &expr, std::string *error = nullptr);
  std::string s_expression() { return s_expression_; };

private:
  struct gcl_envStructure *env_ = nullptr;
  std::string s_expression_ = "";
};

} // namespace cottontail
#endif // COTTONTAIL_SRC_MT_H_
