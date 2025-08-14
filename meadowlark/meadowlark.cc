#include "meadowlark/meadowlark.h"

#include <cassert>
#include <iostream>
#include <memory>
#include <mutex>
#include <regex>
#include <sstream>
#include <string>
#include <thread>

#include "src/core.h"
#include "src/json.h"
#include "src/warren.h"

namespace cottontail {
namespace meadowlark {

bool append_path(std::shared_ptr<Warren> warren, const std::string &filename,
                 addr *path_feature, std::string *error) {
  assert(warren != nullptr);
  warren->start();
  std::string path;
  if (filename.find("/") != std::string::npos)
    path = filename;
  else
    path = "./" + filename;
  if (!warren->transaction(error))
    return false;
  addr p, q;
  if (!warren->appender()->append(path, &p, &q, error) ||
      !warren->annotator()->annotate(warren->featurizer()->featurize("/"), p, q,
                                     error) ||
      !warren->ready(error)) {
    warren->abort();
    warren->end();
    return false;
  }
  warren->commit();
  *path_feature = warren->featurizer()->featurize(path);
  warren->end();
  return true;
}

bool append_jsonl(std::shared_ptr<Warren> warren, const std::string &filename,
                  std::string *error, size_t threads) {
  assert(warren != nullptr);
  std::ifstream f(filename, std::istream::in);
  if (f.fail()) {
    safe_set(error) = "Cannot open: " + filename;
    return false;
  }
  addr path_feature;
  if (!append_path(warren, filename, &path_feature, error))
    return false;
  bool done = false;
  bool failed = false;
  std::mutex sync;
  auto append_worker = [&]() {
    std::shared_ptr<cottontail::Warren> twarren;
    {
      std::lock_guard<std::mutex> _(sync);
      if (done)
        return;
      twarren = warren->clone(error);
      if (twarren == nullptr) {
        done = failed = true;
        return;
      }
    }
    std::string terror;
    twarren->start();
    std::shared_ptr<Scribe> scribe = Scribe::make(twarren, &terror);
    if (scribe == nullptr) {
      twarren->end();
      {
        std::lock_guard<std::mutex> _(sync);
        if (!done) {
          done = failed = true;
          *error = terror;
        }
        return;
      }
    }
    for (;;) {
      std::string line;
      {
        std::lock_guard<std::mutex> _(sync);
        if (done) {
          twarren->end();
          return;
        }
        if (!std::getline(f, line)) {
          twarren->end();
          if (!done)
            done = true;
          return;
        }
      }
      addr p, q;
      if (!scribe->transaction(&terror) ||
          !json_scribe(line, scribe, &p, &q, &terror) ||
          !scribe->annotator()->annotate(path_feature, p, q, &terror) ||
          !scribe->ready(&terror)) {
        scribe->abort();
        twarren->end();
        {
          std::lock_guard<std::mutex> _(sync);
          if (!done) {
            done = failed = true;
            *error = terror;
          }
          return;
        }
      }
      scribe->commit();
    }
  };
  if (threads == 0)
    threads = 2 * std::thread::hardware_concurrency();
  std::vector<std::thread> workers;
  for (size_t i = 0; i < threads; i++)
    workers.emplace_back(std::thread(append_worker));
  for (auto &worker : workers)
    worker.join();
  return !failed;
}

namespace {
size_t lines(std::shared_ptr<std::string> s) {
  size_t count = 0;
  for (char c : *s) {
    if (c == '\n')
      ++count;
  }
  return count;
}
constexpr size_t SMALL = 64 * 1024;
} // namespace

bool append_tsv(std::shared_ptr<Warren> warren, const std::string &filename,
                std::string *error, bool header, const std::string &separator,
                size_t threads) {
  assert(warren != nullptr);
  std::shared_ptr<std::string> contents = inhale(filename, error);
  if (contents == nullptr)
    return false;
  addr path_feature;
  if (!append_path(warren, filename, &path_feature, error))
    return false;
  if (lines(contents) <= SMALL)
    threads = 1;
  if (threads == 0)
    threads = std::thread::hardware_concurrency() + 1;
  std::string sep = separator;
  if (sep == "")
    sep = "[\t]";
  std::regex delim(sep);
  std::vector<addr> tags;
  std::istringstream in(*contents);
  if (header) {
    std::string line;
    if (!std::getline(in, line)) {
      std::sregex_token_iterator it(line.begin(), line.end(), delim, -1), end;
      std::vector<std::string> fields(it, end);
      for (auto &field : fields) {
        std::replace_if(field.begin(), field.end(), ::isspace, '_');
        field = ":" + field + ":";
        tags.push_back(warren->featurizer()->featurize(field));
      }
      warren->start();
      if (!warren->transaction(error))
        return false;
      addr p, q;
      if (!warren->appender()->append(line, &p, &q, error) ||
          !warren->annotator()->annotate(path_feature, p, q, error) ||
          !warren->ready(error)) {
        warren->abort();
        warren->end();
        return false;
      }
    } else {
      safe_error(error) = "Missing header in: " + filename;
      return false;
    }
  }
  bool done = false;
  bool failed = false;
  std::mutex sync;
  auto append_worker = [&]() {
    std::string terror;
    std::shared_ptr<cottontail::Warren> twarren;
    {
      std::lock_guard<std::mutex> _(sync);
      if (done)
        return;
      twarren = warren->clone(error);
      if (twarren == nullptr) {
        done = failed = true;
        return;
      }
      twarren->start();
      if (!twarren->transaction(&terror)) {
        twarren->end();
        done = failed = true;
        return;
      }
    }
    addr record_feature = twarren->featurizer()->featurize(":");
    addr p = maxfinity, q = minfinity;
    for (;;) {
      std::vector<std::string> fields;
      addr p0 = maxfinity, q0 = minfinity;
      std::string line;
      {
        std::lock_guard<std::mutex> _(sync);
        if (done || !std::getline(in, line)) {
          done = true;
          break;
        }
      }
      std::sregex_token_iterator it(line.begin(), line.end(), delim, -1), end;
      fields.assign(it, end);
      while (fields.size() > tags.size())
        tags.push_back(warren->featurizer()->featurize(
            ":" + std::to_string(tags.size()) + ":"));
      for (size_t i = 0; i < fields.size(); i++) {
        addr p1, q1;
        if (!twarren->appender()->append(fields[i], &p1, &q1, &terror) ||
            !twarren->annotator()->annotate(tags[i], p1, q1, &terror)) {
          twarren->abort();
          twarren->end();
          std::lock_guard<std::mutex> _(sync);
          if (!failed) {
            done = failed = true;
            safe_error(error) = terror;
          }
          return;
        }
        p0 = std::min(p0, p1);
        q0 = std::max(q0, q1);
      }
      if (!twarren->annotator()->annotate(record_feature, p0, q0, &terror)) {
        twarren->abort();
        twarren->end();
        std::lock_guard<std::mutex> _(sync);
        if (!failed) {
          done = failed = true;
          safe_error(error) = terror;
        }
        return;
      }
      p = std::min(p, p0);
      q = std::max(q, q0);
    }
    if (p <= q &&
        !twarren->annotator()->annotate(path_feature, p, q, &terror)) {
      twarren->abort();
      twarren->end();
      std::lock_guard<std::mutex> _(sync);
      if (!failed) {
        done = failed = true;
        safe_error(error) = terror;
      }
      return;
    }
    if (!twarren->ready(&terror)) {
      twarren->abort();
      twarren->end();
      std::lock_guard<std::mutex> _(sync);
      if (!failed) {
        done = failed = true;
        safe_error(error) = terror;
      }
      return;
    }
    twarren->commit();
    twarren->end();
  };
  std::vector<std::thread> workers;
  for (size_t i = 0; i < threads; i++)
    workers.emplace_back(std::thread(append_worker));
  for (auto &worker : workers)
    worker.join();
  return !failed;
}
} // namespace meadowlark
} // namespace cottontail
