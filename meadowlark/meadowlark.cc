#include "meadowlark/meadowlark.h"

#include <atomic>
#include <cassert>
#include <cctype>
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
#include "src/builder.h"
#include "src/core.h"
#include "src/json.h"
#include "src/warren.h"

namespace cottontail {
namespace meadowlark {

bool is_meadow(std::shared_ptr<Warren> warren, std::string *error) {
  std::string value;
  if (warren->get_parameter("format", &value) && value == "meadowlark")
    return true;
  safe_set(error) = "Not a meadow";
  return false;
}

const std::string DEFAULT_MEADOW = "a.meadow";

std::shared_ptr<Warren> create_meadow(const std::string &meadow,
                                      std::string *error) {
  std::string options =
      "tokenizer:name:utf8 featurizer@json idx:fvalue_compressor:zlib "
      "idx:posting_compressor:post txt:compressor:zlib ";
  std::shared_ptr<Warren> bigwig = Bigwig::make(meadow, options, error);
  if (bigwig == nullptr ||
      !bigwig->set_parameter("format", "meadowlark", error))
    return nullptr;
  else
    return bigwig;
}

std::shared_ptr<Warren> create_meadow(std::string *error) {
  return create_meadow(DEFAULT_MEADOW, error);
}

std::shared_ptr<Warren> open_meadow(const std::string &meadow,
                                    std::string *error) {
  std::shared_ptr<Warren> warren = Warren::make(meadow, error);
  warren->start();
  if (is_meadow(warren, error)) {
    warren->end();
    return warren;
  } else {
    warren->end();
    return nullptr;
  }
}

std::shared_ptr<Warren> open_meadow(std::string *error) {
  return open_meadow(DEFAULT_MEADOW, error);
}

namespace {
std::string normalized_path(const std::string &filename) {
  if (filename.find("/") != std::string::npos)
    return filename;
  else
    return "./" + filename;
}

std::string gcl_string(const std::string &s) {
  std::string quoted = "\"";
  for (char c : s) {
    if (c == '\\' || c == '"')
      quoted += '\\';
    quoted += c;
  }
  quoted += '"';
  return quoted;
}

bool path_match(const std::string &text, const std::string &path) {
  size_t start = 0;
  while (start < text.size() &&
         std::isspace(static_cast<unsigned char>(text[start])))
    start++;
  if (text.compare(start, path.size(), path) != 0)
    return false;
  size_t end = start + path.size();
  if (end < text.size() &&
      !std::isspace(static_cast<unsigned char>(text[end])))
    return false;
  while (end < text.size() &&
         std::isspace(static_cast<unsigned char>(text[end])))
    end++;
  return true;
}

bool append_path(std::shared_ptr<Appender> appender,
                 std::shared_ptr<Annotator> annotator,
                 std::shared_ptr<Featurizer> featurizer,
                 const std::string &filename, addr *path_feature,
                 std::string *error) {
  assert(appender != nullptr);
  assert(annotator != nullptr);
  assert(featurizer != nullptr);
  assert(path_feature != nullptr);
  std::string path = normalized_path(filename);
  addr p, q;
  if (!appender->append(path, &p, &q, error) ||
      !annotator->annotate(featurizer->featurize("/"), p, q, error))
    return false;
  *path_feature = featurizer->featurize(path);
  return true;
}

bool append_path_scribe(std::shared_ptr<Scribe> scribe,
                        const std::string &filename, addr *path_feature,
                        std::string *error) {
  assert(scribe != nullptr);
  assert(path_feature != nullptr);
  if (!scribe->transaction(error))
    return false;
  if (!append_path(scribe->appender(), scribe->annotator(),
                   scribe->featurizer(), filename, path_feature, error) ||
      !scribe->ready(error)) {
    scribe->abort();
    return false;
  }
  return true;
}

bool append_path_warren(std::shared_ptr<Warren> warren,
                        const std::string &filename, addr *path_feature,
                        std::string *error) {
  assert(warren != nullptr);
  assert(path_feature != nullptr);
  if (!warren->transaction(error))
    return false;
  if (!append_path(warren->appender(), warren->annotator(),
                   warren->featurizer(), filename, path_feature, error) ||
      !warren->ready(error)) {
    warren->abort();
    return false;
  }
  return true;
}
} // namespace

bool already_appended(std::shared_ptr<Warren> warren,
                      const std::string &filename, bool *appended,
                      std::string *error) {
  assert(warren != nullptr);
  assert(appended != nullptr);
  *appended = false;
  std::string path = normalized_path(filename);
  std::unique_ptr<Hopper> hopper =
      warren->hopper_from_gcl("(>> / " + gcl_string(path) + ")", error);
  if (hopper == nullptr)
    return false;
  addr p, q;
  for (hopper->tau(minfinity + 1, &p, &q); p < maxfinity;
       hopper->tau(p + 1, &p, &q)) {
    if (path_match(warren->txt()->translate(p, q), path)) {
      *appended = true;
      return true;
    }
  }
  return true;
}

bool append_jsonl(std::shared_ptr<Warren> warren, const std::string &filename,
                  std::string *error, size_t threads, bool verbose) {
  assert(warren != nullptr);
  warren->start();
  auto finish = [&](bool result) {
    warren->end();
    return result;
  };
  if (verbose)
    std::cerr << "Appending " << filename << "\n" << std::flush;
  if (threads == 0)
    threads = std::thread::hardware_concurrency() + 1;
  std::unique_ptr<std::istream> input = maybe_zipped(filename, error);
  if (input == nullptr)
    return finish(false);
  addr path_feature;
  std::shared_ptr<Scribe> path_scribe = Scribe::make(warren, error);
  if (path_scribe == nullptr)
    return finish(false);
  if (!append_path_scribe(path_scribe, filename, &path_feature, error))
    return finish(false);
  std::vector<std::shared_ptr<cottontail::Warren>> clones;
  std::vector<std::shared_ptr<Scribe>> scribes;
  for (size_t i = 0; i < threads; i++) {
    std::shared_ptr<cottontail::Warren> clone = warren->clone(error);
    if (clone == nullptr) {
      for (size_t j = 0; j < i; j++) {
        scribes[j]->abort();
        clones[j]->end();
      }
      path_scribe->abort();
      return finish(false);
    }
    std::shared_ptr<Scribe> scribe = Scribe::make(clone, error);
    if (scribe == nullptr || !scribe->transaction(error)) {
      clone->end();
      for (size_t j = 0; j < i; j++) {
        scribes[j]->abort();
        clones[j]->end();
      }
      path_scribe->abort();
      return finish(false);
    }
    clones.push_back(clone);
    scribes.push_back(scribe);
  }
  scribes.push_back(path_scribe);
  bool done = false;
  bool failed = false;
  std::mutex sync;
  auto append_worker = [&](size_t n) {
    std::string terror;
    std::shared_ptr<Scribe> scribe = scribes[n];
    for (;;) {
      std::string line;
      {
        std::lock_guard<std::mutex> _(sync);
        if (done)
          break;
        if (!std::getline(*input, line)) {
          done = true;
          if (!input->eof()) {
            safe_error(error) = "Read error on: " + filename;
            failed = true;
            return;
          }
          break;
        }
      }
      addr p, q;
      if (!json_scribe(line, scribe, &p, &q, &terror) ||
          (p <= q &&
           !scribe->annotator()->annotate(path_feature, p, q, &terror))) {
        std::lock_guard<std::mutex> _(sync);
        if (!failed) {
          done = failed = true;
          safe_set(error) = terror;
        }
        return;
      }
    }
    if (!scribe->ready(&terror)) {
      std::lock_guard<std::mutex> _(sync);
      if (!failed) {
        done = failed = true;
        safe_set(error) = terror;
      }
    }
  };
  std::vector<std::thread> workers;
  for (size_t i = 0; i < threads; i++)
    workers.emplace_back(std::thread(append_worker, i));
  for (auto &worker : workers)
    worker.join();
  if (failed) {
    for (auto &scribe : scribes)
      scribe->abort();
    for (size_t i = 0; i < threads; i++)
      clones[i]->end();
  } else {
    Scribe::commit_all(scribes);
    for (auto &scribe : scribes)
      if (!scribe->finalize(error))
        failed = true;
    for (size_t i = 0; i < threads; i++)
      clones[i]->end();
  }
  return finish(!failed);
}

namespace {
constexpr size_t SMALL = 64 * 1024;
size_t thread_count(size_t threads, size_t count) {
  if (count <= SMALL)
    threads = 1;
  if (threads == 0)
    threads = std::thread::hardware_concurrency() + 1;
  return threads;
}
} // namespace

bool append_tsv(std::shared_ptr<Warren> warren, const std::string &filename,
                std::string *error, bool header, std::string separator,
                size_t threads) {
  assert(warren != nullptr);
  std::vector<std::string> lines;
  {
    std::shared_ptr<std::string> contents = inhale(filename, error);
    if (contents == nullptr)
      return false;
    lines = split_lines(*contents);
  }
  addr path_feature;
  if (!append_path_warren(warren, filename, &path_feature, error))
    return false;
  warren->commit();
  if (lines.size() == 0)
    return true;
  threads = thread_count(threads, lines.size());
  std::string wsep = separator;
  if (wsep == "")
    wsep = "\t";
  std::vector<std::shared_ptr<cottontail::Warren>> clones;
  for (size_t i = 0; i < threads; i++) {
    std::shared_ptr<cottontail::Warren> clone = warren->clone(error);
    if (clone == nullptr) {
      for (auto &c : clones) {
        c->abort();
        c->end();
      }
      return false;
    }
    clone->start();
    if (!clone->transaction(error)) {
      clone->end();
      for (auto &c : clones) {
        c->abort();
        c->end();
      }
      return false;
    }
    clones.push_back(clone);
  }
  std::mutex sync_error;
  std::atomic<bool> failed(false);
  auto append_worker = [&](size_t n) {
    std::string terror;
    std::shared_ptr<cottontail::Warren> twarren = clones[n];
    addr data_feature = twarren->featurizer()->featurize(":");
    addr header_feature = twarren->featurizer()->featurize("::");
    std::vector<addr> tags;
    addr p = maxfinity, q = minfinity;
    size_t start = (lines.size() * n) / threads;
    size_t end = (lines.size() * (n + 1)) / threads;
    for (size_t i = start; i < end; i++) {
      addr record_feature;
      if (header && i == 0)
        record_feature = header_feature;
      else
        record_feature = data_feature;
      if (failed.load(std::memory_order_relaxed))
        return;
      addr p0 = maxfinity, q0 = minfinity;
      std::vector<std::string> fields = split_tsv(lines[i], separator);
      while (fields.size() > tags.size())
        tags.push_back(twarren->featurizer()->featurize(
            ":" + std::to_string(tags.size()) + ":"));
      for (size_t j = 0; j < fields.size(); j++) {
        addr p1, q1;
        std::string field = fields[j];
        if (j + 1 < fields.size())
          field += wsep;
        if (!twarren->appender()->append(field, &p1, &q1, &terror) ||
            (p1 <= q1 &&
             !twarren->annotator()->annotate(tags[j], p1, q1, &terror))) {
          if (!failed.exchange(true, std::memory_order_relaxed)) {
            std::lock_guard<std::mutex> _(sync_error);
            safe_error(error) = terror;
          }
          return;
        }
        p0 = std::min(p0, p1);
        q0 = std::max(q0, q1);
      }
      if (p0 <= q0 &&
          !twarren->annotator()->annotate(record_feature, p0, q0, &terror)) {
        if (!failed.exchange(true, std::memory_order_relaxed)) {
          std::lock_guard<std::mutex> _(sync_error);
          safe_error(error) = terror;
        }
        return;
      }
      p = std::min(p, p0);
      q = std::max(q, q0);
    }
    if ((p <= q &&
         !twarren->annotator()->annotate(path_feature, p, q, &terror)) ||
        !twarren->ready(&terror)) {
      if (!failed.exchange(true, std::memory_order_relaxed)) {
        std::lock_guard<std::mutex> _(sync_error);
        safe_error(error) = terror;
      }
    }
  };
  std::vector<std::thread> workers;
  for (size_t i = 0; i < threads; i++)
    workers.emplace_back(std::thread(append_worker, i));
  for (auto &worker : workers)
    worker.join();
  if (failed.load(std::memory_order_relaxed)) {
    for (auto &c : clones) {
      c->abort();
      c->end();
    }
    return false;
  }
  for (auto &c : clones) {
    c->commit();
    c->end();
  }
  return true;
}

bool forage(std::shared_ptr<Warren> warren,
            const std::vector<std::pair<addr, addr>> &intervals,
            const std::string &name, const std::string &tag,
            const std::map<std::string, std::string> &parameters,
            std::string *error, size_t threads) {
  assert(warren != nullptr);
  if (intervals.size() == 0)
    return true;
  if (!Forager::check(name, tag, parameters, error))
    return false;
  threads = thread_count(threads, intervals.size());
  std::map<std::string, std::string> params = parameters;
  params["start"] = std::to_string(intervals[0].first);
  params["end"] = std::to_string(intervals[intervals.size() - 1].second);
  bool failed = false;
  std::mutex sync;
  auto worker = [&](const std::vector<std::pair<addr, addr>> &intervals,
                    size_t start, size_t n) {
    std::string terror;
    std::shared_ptr<cottontail::Warren> twarren;
    {
      std::lock_guard<std::mutex> _(sync);
      if (failed)
        return;
      twarren = warren->clone(error);
      if (twarren == nullptr) {
        failed = true;
        safe_error(error) = terror;
        return;
      }
    }
    twarren->start();
    std::shared_ptr<Forager> forager =
        Forager::make(twarren, name, tag, params, &terror);
    if (!forager->transaction(&terror)) {
      std::lock_guard<std::mutex> _(sync);
      twarren->end();
      if (!failed) {
        failed = true;
        safe_error(error) = terror;
      }
      return;
    }
    if ((start == 0 && !forager->label(&terror)) ||
        !forager->forage(intervals, start, n, &terror) || !forager->ready()) {
      std::lock_guard<std::mutex> _(sync);
      forager->abort();
      twarren->end();
      if (!failed) {
        failed = true;
        safe_error(error) = terror;
      }
      return;
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
  std::map<std::string, std::string> params = parameters;
  params["gcl"] = gcl;
  return forage(warren, intervals, name, tag, params, error, threads);
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
