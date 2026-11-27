#ifndef COTTONTAIL_SRC_MONITOR_H_
#define COTTONTAIL_SRC_MONITOR_H_

#include <memory>
#include <string>
#include <vector>

namespace cottontail {

class Monitor final {
public:
  // Start watching before inventorying existing files. The inventory itself
  // is silent. Skip symlinks. Each ignore entry is a literal name relative
  // to the watched root, e.g. "/.meadow". Ignoring a directory skips its
  // entire subtree. "/" denotes the root itself.
  static std::shared_ptr<Monitor> make(const std::string &path,
                                       const std::vector<std::string> &ignore,
                                       std::string *error = nullptr);
  static std::shared_ptr<Monitor> make(const std::string &path,
                                       std::string *error = nullptr) {
    return make(path, {}, error);
  }

  // Wait for and pop a current or former regular file, spelled /path relative
  // to the watched root. Directory changes report the affected file paths.
  // An empty string means the monitor has stopped.
  std::string next();

  // Stop watching and wake a waiting consumer.
  void stop();

  ~Monitor();
  Monitor(const Monitor &) = delete;
  Monitor &operator=(const Monitor &) = delete;

private:
  Monitor(const std::string &directory, const std::vector<std::string> &ignore);
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace cottontail

#endif // COTTONTAIL_SRC_MONITOR_H_
