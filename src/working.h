#ifndef COTTONTAIL_SRC_WORKING_H_
#define COTTONTAIL_SRC_WORKING_H_

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <vector>

#include "src/core.h"

namespace cottontail {

class Reader {
public:
  static std::shared_ptr<Reader> make(const std::string &name,
                                      const std::string &recipe,
                                      std::string *error = nullptr);
  inline static std::shared_ptr<Reader> make(const std::string &name, bool load,
                                             std::string *error = nullptr) {
    std::string recipe;
    if (load)
      recipe = "full";
    else
      recipe = "file";
    return make(name, recipe, error);
  };
  inline size_t read(char *buffer, size_t where, size_t amount) {
    return read_(buffer, where, amount);
  };
  inline size_t read(char *buffer, size_t amount) {
    return read_(buffer, amount);
  };
  inline size_t size() { return size_(); }

  virtual ~Reader(){};
  Reader(const Reader &) = delete;
  Reader &operator=(const Reader &) = delete;
  Reader(Reader &&) = delete;
  Reader &operator=(Reader &&) = delete;

protected:
  Reader(){};

private:
  virtual size_t read_(char *buffer, size_t where, size_t amount) = 0;
  virtual size_t read_(char *buffer, size_t amount) = 0;
  virtual size_t size_() = 0;
};

class Writer {
public:
  static std::shared_ptr<Writer> make(const std::string &name,
                                      const std::string &recipe,
                                      std::string *error = nullptr);
  inline void append(const char *buffer, size_t amount) {
    return append_(buffer, amount);
  }

  virtual ~Writer(){};
  Writer(const Writer &) = delete;
  Writer &operator=(const Writer &) = delete;
  Writer(Writer &&) = delete;
  Writer &operator=(Writer &&) = delete;

protected:
  Writer(){};

private:
  virtual void append_(const char *buffer, size_t amount) = 0;
};

class Working final {
public:
  static std::shared_ptr<Working> make(const std::string &working_name,
                                       std::string *error = nullptr) {
    std::shared_ptr<Working> working =
        std::make_shared<Working>(working_name, false);
    if (working == nullptr)
      safe_set(error) = "Can't access working directory: " + working_name;
    return working;
  }
  static std::shared_ptr<Working> mkdir(const std::string &working_name,
                                        std::string *error = nullptr) {
    std::shared_ptr<Working> working =
        std::make_shared<Working>(working_name, true);
    if (working == nullptr)
      safe_set(error) = "Can't access working directory: " + working_name;
    return working;
  }
  Working(const std::string &working, bool mkdir)
      : working_(working), temp_counter_(0) {
    if (mkdir) {
      // TODO: Replace with direct function calls. Maybe boost can do this?
      std::string mkdir_command = "/bin/mkdir -p " + working;
      system(mkdir_command.c_str());
      std::string cleanup_command = "/bin/rm -f " + working + "/temp.*";
      system(cleanup_command.c_str());
    }
  }
  std::string make_name(const std::string &name) {
    return working_ + "/" + name;
  }
  std::string make_temp(const std::string &extension = "") {
    lock_.lock();
    std::stringstream ss;
    ss << "temp.";
    ss.fill('0');
    ss.width(8);
    ss << temp_counter_;
    temp_counter_++;
    if (extension != "") {
      ss << ".";
      ss << extension;
    }
    lock_.unlock();
    return make_name(ss.str());
  };
  void remove(const std::string &name) {
    lock_.lock();
    std::string fullname = make_name(name);
    std::remove(fullname.c_str());
    lock_.unlock();
  };
  void load(const std::string &name) {
    lock_.lock();
    load_.insert(name);
    lock_.unlock();
  };
  std::vector<std::string> ls(const std::string &prefix);
  std::shared_ptr<Reader> reader(const std::string &name,
                                 std::string *error = nullptr) {
    std::string fullname = make_name(name);
    return Reader::make(fullname, load_.find(name) != load_.end(), error);
  };
  std::shared_ptr<Reader> reader(const std::string &name,
                                 const std::string &recipe,
                                 std::string *error = nullptr) {
    std::string fullname = make_name(name);
    return Reader::make(fullname, recipe, error);
  };
  std::shared_ptr<Writer> writer(const std::string &name,
                                 const std::string &recipe,
                                 std::string *error = nullptr) {
    std::string fullname = make_name(name);
    return Writer::make(fullname, recipe, error);
  };

private:
  Working(){};
  std::mutex lock_;
  std::set<std::string> load_;
  std::string working_;
  unsigned temp_counter_;
};
} // namespace cottontail
#endif // COTTONTAIL_SRC_WORKING_H_
