#include "meadowlark/meadowlark.h"

#include <cassert>
#include <iostream>
#include <memory>
#include <mutex>
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
  if (!warren->appender()->append(json_encode(path), &p, &q, error) ||
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

} // namespace meadowlark
} // namespace cottontail
