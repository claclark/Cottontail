#include "src/wlock.h"

#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <map>
#include <string>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "src/core.h"
#include "src/dna.h"
#include "src/recipe.h"

namespace cottontail {
namespace {

const std::string LOCK_NAME = "burrow.lock";
const std::string PID_NAME = "burrow.pid";

bool burrow_name(const std::string &burrow, std::string *name) {
  std::string dna;
  std::string error;
  std::map<std::string, std::string> parameters;
  if (!read_dna(burrow + "/" + DNA_NAME, &dna, &error) ||
      !cook(dna, &parameters, &error))
    return false;
  auto item = parameters.find("warren");
  *name =
      item == parameters.end() || item->second == "" ? "simple" : item->second;
  return true;
}

bool directory_entries(const std::string &burrow,
                       std::vector<std::string> *entries, std::string *error) {
  DIR *directory = opendir(burrow.c_str());
  if (directory == nullptr) {
    safe_error(error) = "Cottontail cannot read directory: " + burrow + ": " +
                        std::strerror(errno);
    return false;
  }
  entries->clear();
  int read_error = 0;
  for (;;) {
    errno = 0;
    struct dirent *entry = readdir(directory);
    if (entry == nullptr) {
      read_error = errno;
      break;
    }
    std::string name = entry->d_name;
    if (name != "." && name != "..")
      entries->push_back(name);
  }
  int close_status = closedir(directory);
  if (read_error != 0 || close_status != 0) {
    int directory_error = read_error != 0 ? read_error : errno;
    safe_error(error) = "Cottontail cannot read directory: " + burrow + ": " +
                        std::strerror(directory_error);
    return false;
  }
  return true;
}

int create_lock_file(const std::string &filename, std::string *error) {
  int fd = open(filename.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0666);
  if (fd < 0)
    safe_error(error) =
        "Cottontail cannot create burrow lock file: " + filename + ": " +
        std::strerror(errno);
  return fd;
}

int open_pid_file(const std::string &filename, int flags, std::string *error) {
  int fd = open(filename.c_str(), flags | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK,
                0666);
  if (fd < 0) {
    safe_error(error) = "Cottontail cannot open burrow PID file: " + filename +
                        ": " + std::strerror(errno);
    return -1;
  }
  struct stat status;
  if (fstat(fd, &status) != 0 || !S_ISREG(status.st_mode)) {
    close(fd);
    safe_error(error) = "Burrow PID file is not a regular file: " + filename;
    return -1;
  }
  return fd;
}

} // namespace

WLock::~WLock() {
  if (fd_ >= 0)
    close(fd_);
}

WLock::Status WLock::status() {
  std::lock_guard<std::mutex> lock(status_mutex_);
  return status_;
}

void WLock::initialized() {
  std::lock_guard<std::mutex> lock(status_mutex_);
  if (status_ == Status::created || status_ == Status::recreated)
    status_ = Status::initialized;
}

bool WLock::begin_activation(std::string *error) {
  std::lock_guard<std::mutex> lock(status_mutex_);
  switch (status_) {
  case Status::obtained:
  case Status::initialized:
    status_ = Status::activating;
    return true;
  case Status::created:
  case Status::recreated:
    safe_error(error) = "Burrow must be initialized before activation";
    break;
  case Status::activating:
    safe_error(error) = "Burrow activation is already in progress";
    break;
  case Status::activated:
    safe_error(error) = "Burrow lock has already activated a Warren";
    break;
  case Status::failed:
    safe_set(error) = activation_error_;
    break;
  }
  return false;
}

void WLock::finish_activation(const std::string &error) {
  std::lock_guard<std::mutex> lock(status_mutex_);
  activation_error_ = error;
  status_ = error.empty() ? Status::activated : Status::failed;
}

std::shared_ptr<WLock> WLock::obtain(const std::string &burrow,
                                     std::string *error, bool *busy) {
  if (busy != nullptr)
    *busy = false;
  std::string the_burrow = burrow == "" ? DEFAULT_BURROW : burrow;
  struct stat status;
  if (stat(the_burrow.c_str(), &status) != 0 || !S_ISDIR(status.st_mode)) {
    safe_error(error) = "Not a Cottontail burrow directory: " + the_burrow;
    return nullptr;
  }

  std::shared_ptr<WLock> wlock = std::shared_ptr<WLock>(new WLock());
  std::string lock_filename = the_burrow + "/" + LOCK_NAME;
  wlock->fd_ = open(lock_filename.c_str(), O_RDWR | O_CLOEXEC);
  if (wlock->fd_ < 0) {
    if (errno != ENOENT) {
      safe_error(error) =
          "Cottontail cannot open burrow lock file: " + lock_filename + ": " +
          std::strerror(errno);
      return nullptr;
    }
    std::string name;
    if (!burrow_name(the_burrow, &name)) {
      safe_error(error) = "Directory is not a Cottontail burrow: " + the_burrow;
      return nullptr;
    }
    if (name == "simple") {
      wlock->name_ = name;
      wlock->working_ = Working::make(the_burrow, error);
      if (wlock->working_ == nullptr)
        return nullptr;
      return wlock;
    }
    wlock->fd_ = create_lock_file(lock_filename, error);
    if (wlock->fd_ < 0)
      return nullptr;
  }

  std::string pid_filename = the_burrow + "/" + PID_NAME;
  bool pid_missing = false;
  if (lstat(pid_filename.c_str(), &status) != 0) {
    if (errno != ENOENT) {
      safe_error(error) = "Cottontail cannot inspect burrow PID file: " +
                          pid_filename + ": " + std::strerror(errno);
      return nullptr;
    }
    pid_missing = true;
  } else if (!S_ISREG(status.st_mode)) {
    safe_error(error) = "Burrow PID file is not a regular file: " + pid_filename;
    return nullptr;
  }
  if (flock(wlock->fd_, LOCK_EX | LOCK_NB) != 0) {
    if (errno != EWOULDBLOCK && errno != EAGAIN) {
      safe_error(error) = "Cottontail cannot lock burrow: " + the_burrow +
                          ": " + std::strerror(errno);
      return nullptr;
    }
    if (busy != nullptr)
      *busy = true;
    if (pid_missing)
      sleep(1);
    std::string owner;
    int fd = open_pid_file(pid_filename, O_RDONLY, nullptr);
    if (fd >= 0) {
      char buffer[512];
      ssize_t count = read(fd, buffer, sizeof(buffer));
      close(fd);
      if (count > 0) {
        owner.assign(buffer, count);
        auto newline = owner.find('\n');
        if (newline != std::string::npos)
          owner.resize(newline);
      }
    }
    if (!owner.empty())
      safe_error(error) = "Cottontail burrow locked by: " + owner;
    else
      safe_error(error) = "Cottontail burrow locked";
    return nullptr;
  }

  char hostname[256] = {};
  if (gethostname(hostname, sizeof(hostname) - 1) != 0) {
    safe_error(error) = "Cottontail cannot determine burrow lock host: " +
                        std::string(std::strerror(errno));
    return nullptr;
  }
  int fd = open_pid_file(pid_filename, O_WRONLY | O_CREAT, error);
  if (fd < 0)
    return nullptr;
  std::string owner = std::string(hostname) + "/" + std::to_string(getpid()) +
                      "\n";
  bool written = ftruncate(fd, 0) == 0 &&
                 write(fd, owner.data(), owner.size()) ==
                     static_cast<ssize_t>(owner.size());
  int closed = close(fd);
  if (!written || closed != 0) {
    safe_error(error) =
        "Cottontail cannot write burrow owner information: " + pid_filename;
    return nullptr;
  }
  // Newly created or cleared burrows may not have DNA until initialization.
  burrow_name(the_burrow, &wlock->name_);
  wlock->working_ = Working::make(the_burrow, error);
  if (wlock->working_ == nullptr)
    return nullptr;
  return wlock;
}

std::shared_ptr<WLock> WLock::create(const std::string &name,
                                     const std::string &burrow,
                                     std::string *error) {
  std::string the_burrow = burrow == "" ? DEFAULT_BURROW : burrow;
  struct stat status;
  if (stat(the_burrow.c_str(), &status) != 0) {
    if (errno != ENOENT || mkdir(the_burrow.c_str(), 0777) != 0) {
      safe_error(error) =
          "Cottontail cannot create burrow directory: " + the_burrow + ": " +
          std::strerror(errno);
      return nullptr;
    }
  } else if (!S_ISDIR(status.st_mode)) {
    safe_error(error) = "Not a Cottontail burrow: " + the_burrow;
    return nullptr;
  }
  std::vector<std::string> entries;
  if (!directory_entries(the_burrow, &entries, error))
    return nullptr;
  if (!entries.empty()) {
    safe_error(error) =
        "Cottontail cannot create burrow in nonempty directory: " + the_burrow;
    return nullptr;
  }
  int fd = create_lock_file(the_burrow + "/" + LOCK_NAME, error);
  if (fd < 0)
    return nullptr;
  close(fd);
  std::shared_ptr<WLock> wlock = obtain(the_burrow, error);
  if (wlock == nullptr)
    return nullptr;
  // Another creator may have initialized the burrow before we got the lock.
  if (!directory_entries(the_burrow, &entries, error))
    return nullptr;
  for (const auto &entry : entries) {
    if (entry != LOCK_NAME && entry != PID_NAME) {
      safe_error(error) =
          "Cottontail cannot create burrow in nonempty directory: " + the_burrow;
      return nullptr;
    }
  }
  wlock->name_ = name;
  wlock->status_ = Status::created;
  return wlock;
}

std::shared_ptr<WLock> WLock::recreate(const std::string &name,
                                       const std::string &burrow,
                                       std::string *error) {
  std::string the_burrow = burrow == "" ? DEFAULT_BURROW : burrow;
  struct stat status;
  if (stat(the_burrow.c_str(), &status) != 0) {
    if (errno == ENOENT)
      return create(name, the_burrow, error);
    safe_error(error) = "Cottontail cannot access burrow: " + the_burrow +
                        ": " + std::strerror(errno);
    return nullptr;
  }
  if (!S_ISDIR(status.st_mode)) {
    safe_error(error) = "Not a Cottontail burrow: " + the_burrow;
    return nullptr;
  }
  std::shared_ptr<WLock> wlock = obtain(the_burrow, error);
  if (wlock == nullptr)
    return nullptr;
  if (wlock->name_ == "simple") {
    safe_error(error) = "Cannot reinitialize simple Cottontail burrows";
    return nullptr;
  }
  std::vector<std::string> entries;
  if (!directory_entries(the_burrow, &entries, error))
    return nullptr;
  struct stat directory_status;
  if (stat(the_burrow.c_str(), &directory_status) != 0 ||
      faccessat(AT_FDCWD, the_burrow.c_str(), W_OK | X_OK, AT_EACCESS) != 0) {
    safe_error(error) = "Cottontail cannot remove files from burrow: " +
                        the_burrow + ": " + std::strerror(errno);
    return nullptr;
  }
  // Check all entries before unlinking any burrow data.
  uid_t user = geteuid();
  for (const auto &entry : entries) {
    std::string filename = the_burrow + "/" + entry;
    if (lstat(filename.c_str(), &status) != 0) {
      safe_error(error) = "Cottontail cannot inspect burrow entry: " + filename +
                          ": " + std::strerror(errno);
      return nullptr;
    }
    if (S_ISDIR(status.st_mode)) {
      safe_error(error) = "Burrow contains subdirectories: " + filename;
      return nullptr;
    }
    if (!S_ISREG(status.st_mode)) {
      safe_error(error) = "Burrow contains non-regular files: " + filename;
      return nullptr;
    }
    if (entry == LOCK_NAME || entry == PID_NAME)
      continue;
    if (!(status.st_mode & (S_IWUSR | S_IWGRP | S_IWOTH)) ||
        faccessat(AT_FDCWD, filename.c_str(), W_OK, AT_EACCESS) != 0) {
      safe_error(error) = "Burrow contains read-only files: " + filename;
      return nullptr;
    }
    if ((directory_status.st_mode & S_ISVTX) && user != 0 &&
        directory_status.st_uid != user && status.st_uid != user) {
      safe_error(error) = "Burrow contains files that cannot be removed: " +
                          filename;
      return nullptr;
    }
  }
  for (auto &entry : entries) {
    if (entry == LOCK_NAME || entry == PID_NAME)
      continue;
    std::string filename = the_burrow + "/" + entry;
    if (unlink(filename.c_str()) != 0) {
      safe_error(error) = "Cottontail cannot clear burrow file: " + filename +
                          ": " + std::strerror(errno);
      return nullptr;
    }
  }
  wlock->name_ = name;
  wlock->status_ = Status::recreated;
  return wlock;
}

} // namespace cottontail
