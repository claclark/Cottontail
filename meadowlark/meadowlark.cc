#include "meadowlark/meadowlark.h"

#include <cassert>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <sstream>
#include <string>
#include <thread>

#include "meadowlark/forager.h"
#include "src/bigwig.h"
#include "src/core.h"
#include "src/json.h"
#include "src/warren.h"

namespace cottontail {
namespace meadowlark {

const std::string DEFAULT_MEADOW = "a.meadow";

std::shared_ptr<Warren> create_meadow(const std::string &meadow,
                                      std::string *error) {
  std::string options =
      "tokenizer:name:utf8 featurizer@json idx:fvalue_compressor:zlib "
      "idx:posting_compressor:post txt:compressor:zlib ";
  return Bigwig::make(meadow, options, error);
}

std::shared_ptr<Warren> create_meadow(std::string *error) {
  return create_meadow(DEFAULT_MEADOW, error);
}

std::shared_ptr<Warren> open_meadow(const std::string &meadow,
                                    std::string *error) {
  return Warren::make(meadow, error);
}

std::shared_ptr<Warren> open_meadow(std::string *error) {
  return Warren::make(DEFAULT_MEADOW, error);
}

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
size_t thread_count(size_t threads, size_t count) {
  if (count <= SMALL)
    threads = 1;
  if (threads == 0)
    threads = std::thread::hardware_concurrency() + 1;
  return threads;
}
} // namespace

namespace {
struct Barrier {
  std::mutex mu;
  std::condition_variable cv;
  size_t arrived = 0;
  size_t total;

  explicit Barrier(size_t n) : total(n) {}

  void arrive_and_wait() {
    std::unique_lock<std::mutex> lk(mu);
    if (++arrived == total)
      cv.notify_all();
    else
      cv.wait(lk, [&] { return arrived == total; });
  }
};
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
  threads = thread_count(threads, lines(contents));
  std::string sep = separator;
  if (sep == "")
    sep = "[\t]";
  std::regex delim(sep);
  std::vector<addr> tags;
  std::istringstream in(*contents);
  if (header) {
    std::string line;
    if (std::getline(in, line)) {
      std::sregex_token_iterator it(line.begin(), line.end(), delim, -1), end;
      std::vector<std::string> fields(it, end);
      line = "";
      for (size_t i = 0; i < fields.size(); i++) {
        std::string field = fields[i];
        if (i)
          line += "\t";
        else
          line += "\n";
        line += field;
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
      warren->commit();
      warren->end();
    } else {
      safe_error(error) = "Missing header in: " + filename;
      return false;
    }
  }
  bool done = false;
  bool failed = false;
  std::mutex sync;
  Barrier barrier(threads);
  auto append_worker = [&]() {
    std::vector<addr> ttags = tags;
    std::string terror;
    std::shared_ptr<cottontail::Warren> twarren;
    {
      std::lock_guard<std::mutex> _(sync);
      if (done)
        return;
      twarren = warren->clone(&terror);
      if (twarren == nullptr) {
        done = failed = true;
        safe_error(error) = terror;
        return;
      }
    }
    twarren->start();
    if (!twarren->transaction(&terror)) {
      twarren->end();
      std::lock_guard<std::mutex> _(sync);
      if (failed)
        return;
      done = failed = true;
      safe_error(error) = terror;
      return;
    }
    addr record_feature = twarren->featurizer()->featurize(":");
    addr p = maxfinity, q = minfinity;
    for (;;) {
      std::vector<std::string> fields;
      addr p0 = maxfinity, q0 = minfinity;
      std::string line;
      {
        sync.lock();
        if (failed) {
          sync.unlock();
          twarren->abort();
          twarren->end();
          return;
        }
        if (done) {
          sync.unlock();
          break;
        }
        if (!std::getline(in, line)) {
          done = true;
          if (!in.eof()) {
            failed = true;
            safe_error(error) = "Read error while reading: " + filename;
            sync.unlock();
            twarren->abort();
            twarren->end();
            return;
          }
          sync.unlock();
          break;
        }
        sync.unlock();
      }
      std::sregex_token_iterator it(line.begin(), line.end(), delim, -1), end;
      fields.assign(it, end);
      while (fields.size() > ttags.size())
        ttags.push_back(twarren->featurizer()->featurize(
            ":" + std::to_string(ttags.size()) + ":"));
      for (size_t i = 0; i < fields.size(); i++) {
        addr p1, q1;
        std::string field = fields[i];
        if (i + 1 < fields.size())
          field += "\t";
        else
          field += "\n";
        if (!twarren->appender()->append(field, &p1, &q1, &terror) ||
            !twarren->annotator()->annotate(ttags[i], p1, q1, &terror)) {
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
    // sync threads
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

bool forage(std::shared_ptr<Warren> warren,
            const std::vector<std::pair<addr, addr>> &intervals,
            const std::string &name, const std::string &tag,
            const std::map<std::string, std::string> &parameters,
            std::string *error, size_t threads) {
  if (intervals.size() == 0)
    return true;
  if (!Forager::check(name, tag, parameters, error))
    return false;
  assert(warren != nullptr);
  threads = thread_count(threads, intervals.size());
  bool done = false;
  bool failed = false;
  std::mutex sync;
  auto worker = [&](const std::vector<std::pair<addr, addr>> &intervals,
                    size_t start, size_t n) {
    std::string terror;
    std::shared_ptr<cottontail::Warren> twarren;
    static std::shared_ptr<Forager> forager;
    {
      std::lock_guard<std::mutex> _(sync);
      if (done)
        return;
      twarren = warren->clone(error);
      if (twarren == nullptr) {
        done = failed = true;
        safe_error(error) = terror;
        return;
      }
      twarren->start();
      forager = Forager::make(warren, name, tag, parameters, &terror);
      if (!forager->transaction(&terror)) {
        twarren->end();
        done = failed = true;
        safe_error(error) = terror;
        return;
      }
    }
    if (!forager->forage(intervals, start, n, &terror) || !forager->ready()) {
      std::lock_guard<std::mutex> _(sync);
      forager->abort();
      twarren->end();
      if (!failed) {
        done = failed = true;
        safe_error(error) = terror;
      }
    }
    forager->commit();
    twarren->end();
  };
  size_t start = 0;
  size_t split = intervals.size() / threads;
  std::vector<std::thread> workers;
  for (size_t i = 0; i < threads; i++) {
    size_t n = (i == threads - 1 ? intervals.size() - start : split);
    workers.emplace_back(std::thread(worker, intervals, start, n));
    start += split;
  }
  for (auto &worker : workers)
    worker.join();
  return !failed;
}

bool forage(std::shared_ptr<Warren> warren,
            const std::vector<std::pair<addr, addr>> &intervals,
            const std::string &name, const std::string &tag, std::string *error,
            size_t threads) {
  std::map<std::string, std::string> parameters;
  return forage(warren, intervals, name, tag, parameters, error, threads);
}

bool forage(std::shared_ptr<Warren> warren, const std::string &gcl, addr start,
            addr end, const std::string &name, const std::string &tag,
            const std::map<std::string, std::string> &parameters,
            std::string *error, size_t threads) {
  assert(warren != nullptr);
  warren->start();
  std::shared_ptr<cottontail::Hopper> hopper =
      warren->hopper_from_gcl(gcl, error);
  if (hopper == nullptr) {
    warren->end();
    return false;
  }
  end = (end < maxfinity ? end + 1 : maxfinity);
  addr q, p = (start == minfinity ? minfinity + 1 : start);
  std::vector<std::pair<addr, addr>> intervals;
  for (hopper->tau(p, &p, &q); q < end; hopper->tau(p + 1, &p, &q))
    intervals.emplace_back(p, q);
  warren->end();
  return forage(warren, intervals, name, tag, error, threads);
}

bool forage(std::shared_ptr<Warren> warren, const std::string &gcl,
            const std::string &name, const std::string &tag,
            const std::map<std::string, std::string> &parameters,
            std::string *error, size_t threads) {
  return forage(warren, gcl, minfinity, maxfinity, name, tag, parameters, error,
                threads);
}
} // namespace meadowlark
} // namespace cottontail
