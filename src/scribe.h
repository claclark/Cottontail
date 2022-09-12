#ifndef COTTONTAIL_SRC_SCRIBE_H_
#define COTTONTAIL_SRC_SCRIBE_H_

#include <memory>
#include <string>

#include "src/annotator.h"
#include "src/appender.h"
#include "src/committable.h"
#include "src/core.h"

namespace cottontail {

class Scribe : public Committable {
public:
  static std::shared_ptr<Scribe> null(std::string *error = nullptr);
  inline std::shared_ptr<Annotator> annotator() { return annotator_; };
  inline std::shared_ptr<Appender> appender() { return appender_; };
  inline bool set(const std::string &key, const std::string &value,
                  std::string *error = nullptr) {
    return set_(key, value, error);
  };

  virtual ~Scribe(){};
  Scribe(const Scribe &) = delete;
  Scribe &operator=(const Scribe &) = delete;
  Scribe(Scribe &&) = delete;
  Scribe &operator=(Scribe &&) = delete;

protected:
  Scribe(){};

private:
  std::shared_ptr<Annotator> annotator_;
  std::shared_ptr<Appender> appender_;
  bool set_(const std::string &key, const std::string &value,
            std::string *error);
};

} // namespace cottontail
#endif // COTTONTAIL_SRC_SCRIBE_H_
