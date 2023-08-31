#ifndef COTTONTAIL_SRC_SCRIBE_H_
#define COTTONTAIL_SRC_SCRIBE_H_

#include <memory>
#include <string>

#include "src/annotator.h"
#include "src/appender.h"
#include "src/builder.h"
#include "src/committable.h"
#include "src/core.h"
#include "src/warren.h"

namespace cottontail {

class Scribe : public Committable {
public:
  static std::shared_ptr<Scribe> null(std::string *error = nullptr);
  static std::shared_ptr<Scribe> make(std::shared_ptr<Warren> warren,
                                      std::string *error = nullptr);
  static std::shared_ptr<Scribe> make(std::shared_ptr<Builder> builder,
                                      std::string *error = nullptr);
  inline std::shared_ptr<Featurizer> featurizer() { return featurizer_(); };
  inline std::shared_ptr<Annotator> annotator() { return annotator_(); };
  inline std::shared_ptr<Appender> appender() { return appender_(); };
  inline bool set(const std::string &key, const std::string &value,
                  std::string *error = nullptr) {
    return set_(key, value, error);
  };
  inline bool finalize(std::string *error = nullptr) {
    return finalize_(error);
  }

  virtual ~Scribe(){};
  Scribe(const Scribe &) = delete;
  Scribe &operator=(const Scribe &) = delete;
  Scribe(Scribe &&) = delete;
  Scribe &operator=(Scribe &&) = delete;

protected:
  Scribe(){};

private:
  virtual std::shared_ptr<Featurizer> featurizer_() = 0;
  virtual std::shared_ptr<Annotator> annotator_() = 0;
  virtual std::shared_ptr<Appender> appender_() = 0;
  virtual bool set_(const std::string &key, const std::string &value,
                    std::string *error) = 0;
  virtual bool finalize_(std::string *error) = 0;
};

bool scribe_files(const std::vector<std::string> &filesnames,
                  std::shared_ptr<Scribe> scribe, std::string *error = nullptr,
                  bool verbose = false, addr *p = nullptr, addr *q = nullptr);

} // namespace cottontail
#endif // COTTONTAIL_SRC_SCRIBE_H_
