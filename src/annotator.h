#ifndef COTTONTAIL_SRC_ANNOTATOR_H_
#define COTTONTAIL_SRC_ANNOTATOR_H_

#include <memory>
#include <string>

#include "src/committable.h"
#include "src/core.h"
#include "src/working.h"

namespace cottontail {

class Annotator : public Committable {
public:
  static std::shared_ptr<Annotator>
  make(const std::string &name, const std::string &recipe,
       std::string *error = nullptr,
       std::shared_ptr<Working> working = nullptr);
  static bool check(const std::string &name, const std::string &recipe,
                    std::string *error = nullptr);
  static bool recover(const std::string &name, const std::string &recipe,
                      bool commit, std::string *error = nullptr,
                      std::shared_ptr<Working> working = nullptr);
  inline std::string recipe() { return recipe_(); }
  inline std::string name() { return name_; }
  inline bool annotate(addr feature, addr p, addr q, fval v,
                       std::string *error = nullptr) {
    if (feature == null_feature)
      return true;
    else
      return annotate_(feature, p, q, v, error);
  };
  inline bool annotate(addr feature, addr p, addr q,
                       std::string *error = nullptr) {
    if (feature == null_feature)
      return true;
    else
      return annotate_(feature, p, q, 0.0, error);
  };
  inline bool annotate(addr feature, addr p, addr q, addr v,
                       std::string *error = nullptr) {
    if (feature == null_feature)
      return true;
    else
      return annotate(feature, p, q, addr2fval(v), error);
  };
  inline bool annotate(const std::string &annotations_filename,
                       std::string *error = nullptr) {
    return annotate_(annotations_filename, error);
  }
  inline bool erase(addr p, addr q, std::string *error = nullptr) {
    return erase_(p, q, error);
  }

  virtual ~Annotator(){};
  Annotator(const Annotator &) = delete;
  Annotator &operator=(const Annotator &) = delete;
  Annotator(Annotator &&) = delete;
  Annotator &operator=(Annotator &&) = delete;

protected:
  Annotator(){};

private:
  virtual std::string recipe_() = 0;
  virtual bool annotate_(addr feature, addr p, addr q, fval v,
                         std::string *error) = 0;
  virtual bool annotate_(const std::string &annotations_filename,
                         std::string *error);
  virtual bool erase_(addr p, addr q, std::string *error);
  std::string name_ = "";
};

} // namespace cottontail

#endif // COTTONTAIL_SRC_ANNOTATOR_H_
