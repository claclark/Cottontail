#ifndef COTTONTAIL_SRC_WLOCK_H_
#define COTTONTAIL_SRC_WLOCK_H_

#include <memory>
#include <mutex>
#include <string>

#include "src/working.h"

namespace cottontail {

class WLock final {
public:
  enum class Status {
    obtained,
    created,
    recreated,
    initialized,
    activating,
    activated,
    failed
  };

  // Existing burrows; legacy Simple burrows remain unlocked.
  static std::shared_ptr<WLock> obtain(const std::string &burrow,
                                       std::string *error = nullptr);
  // A missing target or an existing empty directory.
  static std::shared_ptr<WLock> create(const std::string &name,
                                       const std::string &burrow,
                                       std::string *error = nullptr);
  // Clear a recognized burrow, preserving the lock files; refuse Simple.
  static std::shared_ptr<WLock> recreate(const std::string &name,
                                         const std::string &burrow,
                                         std::string *error = nullptr);
  std::string name() { return name_; }
  std::shared_ptr<Working> working() { return working_; }
  Status status();
  // Consume initialization readiness after successfully writing DNA.
  void initialized();

  ~WLock();
  WLock(const WLock &) = delete;
  WLock &operator=(const WLock &) = delete;
  WLock(WLock &&) = delete;
  WLock &operator=(WLock &&) = delete;

private:
  friend class Warren;
  bool begin_activation(std::string *error);
  void finish_activation(const std::string &error);
  WLock(){};
  int fd_ = -1;
  std::mutex status_mutex_;
  Status status_ = Status::obtained;
  std::string activation_error_;
  std::string name_;
  std::shared_ptr<Working> working_;
};

} // namespace cottontail
#endif // COTTONTAIL_SRC_WLOCK_H_
