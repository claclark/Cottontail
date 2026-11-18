#include "src/wlock.h"

#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
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

} // namespace

WLock::~WLock() {
  if (fd_ >= 0)
    close(fd_);
}

std::shared_ptr<WLock> WLock::obtain(const std::string &burrow,
                                     std::string *error) {
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
  if (flock(wlock->fd_, LOCK_EX | LOCK_NB) != 0) {
    if (errno != EWOULDBLOCK && errno != EAGAIN) {
      safe_error(error) = "Cottontail cannot lock burrow: " + the_burrow +
                          ": " + std::strerror(errno);
      return nullptr;
    }
    if (stat(pid_filename.c_str(), &status) != 0 && errno == ENOENT)
      sleep(1);
    std::ifstream in(pid_filename);
    std::string owner;
    if (std::getline(in, owner) && owner != "")
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
  std::ofstream out(pid_filename);
  out << hostname << "/" << getpid() << "\n";
  out.close();
  if (out.fail()) {
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
  if (wlock != nullptr)
    wlock->name_ = name;
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
  return wlock;
}

} // namespace cottontail
