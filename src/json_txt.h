#ifndef COTTONTAIL_SRC_JSON_TXT_H_
#define COTTONTAIL_SRC_JSON_TXT_H_

#include <map>
#include <memory>
#include <string>

#include "src/core.h"
#include "src/txt.h"

namespace cottontail {
class JsonTxt final : public Txt {
public:
  static std::shared_ptr<Txt> wrap(const std::string &recipe,
                                   std::shared_ptr<Txt> txt,
                                   std::string *error = nullptr);
  static std::shared_ptr<Txt>
  wrap(const std::map<std::string, std::string> &parameters,
       std::shared_ptr<Txt> txt, std::string *error = nullptr);
  static std::shared_ptr<Txt> wrap(std::shared_ptr<Txt> txt,
                                   std::string *error = nullptr);

  ~JsonTxt(){};
  JsonTxt(const JsonTxt &) = delete;
  JsonTxt &operator=(const JsonTxt &) = delete;
  JsonTxt(JsonTxt &&) = delete;
  JsonTxt &operator=(JsonTxt &&) = delete;

private:
  JsonTxt(){};
  std::shared_ptr<Txt> txt_;
  bool transaction_(std::string *error) final;
  bool ready_() final;
  void commit_() final;
  void abort_() final;
  std::string name_() final;
  std::string recipe_() final;
  std::shared_ptr<Txt> clone_(std::string *error) final;
  std::string translate_(addr p, addr q) final;
  std::string raw_(addr p, addr q) final;
  addr tokens_() final;
  bool range_(addr *p, addr *q) final;
};
} // namespace cottontail

#endif // COTTONTAIL_SRC_JSON_TXT_H_
