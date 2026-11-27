#ifndef COTTONTAIL_SRC_MONITOR_H_
#define COTTONTAIL_SRC_MONITOR_H_

#include <memory>
#include <string>

namespace cottontail {

class Monitor final {
public:
  // Start watching before inventorying existing files. The inventory itself
  // is silent. Skip symlinks and dot-prefixed components below the root.
  static std::shared_ptr<Monitor> make(const std::string &path,
                                       std::string *error = nullptr);

  // Wait for and pop a current or former regular file, spelled ./path relative
  // to the watched root. Directory changes report the affected file paths.
  // An empty string means the monitor has stopped.
  std::string next();

  // Stop watching and wake a waiting consumer.
  void stop();

  ~Monitor();
  Monitor(const Monitor &) = delete;
  Monitor &operator=(const Monitor &) = delete;

private:
  explicit Monitor(const std::string &directory);
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace cottontail

#endif // COTTONTAIL_SRC_MONITOR_H_
