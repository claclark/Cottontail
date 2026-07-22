#include "meadowlark/meadowlark.h"

#include <cassert>
#include <cctype>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <thread>

#include "meadowlark/forager.h"
#include "meadowlark/metadata.h"
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

bool append_segment_name(std::shared_ptr<Appender> appender,
                         std::shared_ptr<Annotator> annotator,
                         std::shared_ptr<Featurizer> featurizer,
                         const std::string &filename, addr *p, addr *q,
                         std::string *error) {
  assert(appender != nullptr);
  assert(annotator != nullptr);
  assert(featurizer != nullptr);
  assert(p != nullptr);
  assert(q != nullptr);
  std::string path = normalized_path(filename);
  if (!appender->append(path, p, q, error) ||
      !annotator->annotate(featurizer->featurize("//"), *p, *q, error))
    return false;
  return true;
}

bool append_path(std::shared_ptr<Appender> appender,
                 std::shared_ptr<Annotator> annotator,
                 std::shared_ptr<Featurizer> featurizer,
                 const std::string &filename, addr *path_feature, addr *p,
                 addr *q, std::string *error) {
  assert(path_feature != nullptr);
  if (!append_segment_name(appender, annotator, featurizer, filename, p, q,
                           error) ||
      !annotator->annotate(featurizer->featurize("/"), *p, *q, error))
    return false;
  *path_feature = featurizer->featurize(normalized_path(filename));
  return true;
}

bool append_source(std::shared_ptr<Warren> warren,
                   const std::string &filename, const std::string &metadata,
                   addr *path_feature, std::string *error) {
  assert(warren != nullptr);
  assert(path_feature != nullptr);
  if (!warren->transaction(error))
    return false;
  addr source_p, source_q, metadata_p, metadata_q;
  if (!append_path(warren->appender(), warren->annotator(),
                   warren->featurizer(), filename, path_feature, &source_p,
                   &source_q, error) ||
      !json_append(metadata, warren, &metadata_p, &metadata_q, "@", error) ||
      !warren->annotator()->annotate(warren->featurizer()->featurize("/."),
                                     source_p, metadata_q, error) ||
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
  if (!append_source(warren, filename,
                     json_metadata(normalized_path(filename)), &path_feature,
                     error))
    return finish(false);
  std::vector<std::shared_ptr<cottontail::Warren>> clones;
  for (size_t i = 0; i < threads; i++) {
    std::shared_ptr<cottontail::Warren> clone = warren->clone(error);
    if (clone == nullptr) {
      for (auto &c : clones) {
        c->abort();
        c->end();
      }
      warren->abort();
      return finish(false);
    }
    if (!clone->transaction(error)) {
      clone->end();
      for (auto &c : clones) {
        c->abort();
        c->end();
      }
      warren->abort();
      return finish(false);
    }
    clones.push_back(clone);
  }
  bool done = false;
  bool failed = false;
  std::mutex sync;
  auto append_worker = [&](size_t n) {
    std::string terror;
    std::shared_ptr<Warren> twarren = clones[n];
    addr segment_p = maxfinity, segment_q = minfinity;
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
      if (segment_p == maxfinity &&
          !append_segment_name(twarren->appender(), twarren->annotator(),
                               twarren->featurizer(), filename, &segment_p,
                               &segment_q, &terror)) {
        std::lock_guard<std::mutex> _(sync);
        if (!failed) {
          done = failed = true;
          safe_set(error) = terror;
        }
        return;
      }
      addr p, q;
      if (!json_append(line, twarren, &p, &q, ":", &terror) ||
          (p <= q &&
           !twarren->annotator()->annotate(path_feature, p, q, &terror))) {
        std::lock_guard<std::mutex> _(sync);
        if (!failed) {
          done = failed = true;
          safe_set(error) = terror;
        }
        return;
      }
      segment_q = std::max(segment_q, q);
    }
    if ((segment_p <= segment_q &&
         !twarren->annotator()->annotate(
             twarren->featurizer()->featurize("/."), segment_p, segment_q,
             &terror)) ||
        !twarren->ready(&terror)) {
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
    for (auto &clone : clones) {
      clone->abort();
      clone->end();
    }
    warren->abort();
  } else {
    std::vector<std::shared_ptr<Warren>> warrens = clones;
    warrens.push_back(warren);
    Warren::commit_all(warrens);
    for (auto &clone : clones)
      clone->end();
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

std::string numeric_column_feature(size_t index) {
  return ":" + std::to_string(index) + ":";
}

std::string normalized_heading(const std::string &heading) {
  std::string normalized;
  bool whitespace = false;
  for (unsigned char c : heading)
    if (std::isspace(c)) {
      if (!whitespace)
        normalized += '_';
      whitespace = true;
    } else {
      normalized += static_cast<char>(c);
      whitespace = false;
    }
  return normalized;
}

void tsv_columns(const std::string &first_line, bool have_first_line,
                 const std::string &separator, bool header,
                 std::vector<std::string> *headings,
                 std::vector<std::string> *features) {
  assert(headings != nullptr);
  assert(features != nullptr);
  std::vector<std::string> first;
  if (have_first_line)
    first = split_tsv(first_line, separator);
  size_t columns = first.size();
  headings->assign(columns, "");
  if (header)
    *headings = first;
  features->resize(columns);
  std::set<std::string> numeric;
  for (size_t i = 0; i < columns; i++)
    numeric.insert(numeric_column_feature(i));
  std::set<std::string> used;
  for (size_t i = 0; i < columns; i++) {
    std::string fallback = numeric_column_feature(i);
    std::string feature = fallback;
    if (header && (*headings)[i] != "") {
      std::string heading = normalized_heading((*headings)[i]);
      if (heading != "")
        feature = ":" + heading + ":";
      if (used.find(feature) != used.end() ||
          (numeric.find(feature) != numeric.end() && feature != fallback))
        feature = fallback;
    }
    used.insert(feature);
    (*features)[i] = feature;
  }
}
} // namespace

bool append_tsv(std::shared_ptr<Warren> warren, const std::string &filename,
                std::string *error, bool header, std::string separator,
                size_t threads) {
  assert(warren != nullptr);
  warren->start();
  auto finish = [&](bool result) {
    warren->end();
    return result;
  };
  std::unique_ptr<std::istream> input = maybe_zipped(filename, error);
  if (input == nullptr)
    return finish(false);
  std::string first_line;
  bool have_first_line = static_cast<bool>(std::getline(*input, first_line));
  if (!have_first_line && !input->eof()) {
    safe_error(error) = "Read error on: " + filename;
    return finish(false);
  }
  if (!first_line.empty() && first_line.back() == '\r')
    first_line.pop_back();
  std::vector<std::string> headings, column_features;
  tsv_columns(first_line, have_first_line, separator, header, &headings,
              &column_features);
  addr path_feature;
  if (!append_source(warren, filename,
                     tsv_metadata(normalized_path(filename), separator, header,
                                  headings, column_features),
                     &path_feature, error))
    return finish(false);
  if (!have_first_line) {
    Warren::commit_all({warren});
    return finish(true);
  }
  if (threads == 0)
    threads = std::thread::hardware_concurrency() + 1;
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
      warren->abort();
      return finish(false);
    }
    if (!clone->transaction(error)) {
      clone->end();
      for (auto &c : clones) {
        c->abort();
        c->end();
      }
      warren->abort();
      return finish(false);
    }
    clones.push_back(clone);
  }
  bool first_pending = true;
  bool done = false;
  bool failed = false;
  std::mutex sync;
  auto fail = [&](const std::string &message) {
    std::lock_guard<std::mutex> _(sync);
    if (!failed) {
      done = failed = true;
      safe_set(error) = message;
    }
  };
  auto append_worker = [&](size_t n) {
    std::string terror;
    std::shared_ptr<cottontail::Warren> twarren = clones[n];
    addr data_feature = twarren->featurizer()->featurize(":");
    addr header_feature = twarren->featurizer()->featurize("::");
    std::vector<addr> tags;
    for (const auto &feature : column_features)
      tags.push_back(twarren->featurizer()->featurize(feature));
    addr p = maxfinity, q = minfinity;
    addr segment_p = maxfinity, segment_q = minfinity;
    for (;;) {
      std::string line;
      bool header_record = false;
      {
        std::lock_guard<std::mutex> _(sync);
        if (done)
          break;
        if (first_pending) {
          line = first_line;
          first_pending = false;
          header_record = header;
        } else if (!std::getline(*input, line)) {
          done = true;
          if (!input->eof()) {
            failed = true;
            safe_set(error) = "Read error on: " + filename;
          }
          break;
        }
      }
      if (!line.empty() && line.back() == '\r')
        line.pop_back();
      if (segment_p == maxfinity &&
          !append_segment_name(twarren->appender(), twarren->annotator(),
                               twarren->featurizer(), filename, &segment_p,
                               &segment_q, &terror)) {
        fail(terror);
        return;
      }
      addr record_feature = header_record ? header_feature : data_feature;
      addr p0 = maxfinity, q0 = minfinity;
      std::vector<std::string> fields = split_tsv(line, separator);
      while (fields.size() > tags.size())
        tags.push_back(twarren->featurizer()->featurize(
            numeric_column_feature(tags.size())));
      for (size_t j = 0; j < fields.size(); j++) {
        addr p1, q1;
        std::string field = fields[j];
        if (j + 1 < fields.size())
          field += wsep;
        if (!twarren->appender()->append(field, &p1, &q1, &terror) ||
            (p1 <= q1 &&
             !twarren->annotator()->annotate(tags[j], p1, q1, &terror))) {
          fail(terror);
          return;
        }
        p0 = std::min(p0, p1);
        q0 = std::max(q0, q1);
      }
      if (p0 <= q0 &&
          !twarren->annotator()->annotate(record_feature, p0, q0, &terror)) {
        fail(terror);
        return;
      }
      p = std::min(p, p0);
      q = std::max(q, q0);
      segment_q = std::max(segment_q, q0);
    }
    if ((p <= q &&
         !twarren->annotator()->annotate(path_feature, p, q, &terror)) ||
        (segment_p <= segment_q &&
         !twarren->annotator()->annotate(
             twarren->featurizer()->featurize("/."), segment_p, segment_q,
             &terror)) ||
        !twarren->ready(&terror)) {
      fail(terror);
    }
  };
  std::vector<std::thread> workers;
  for (size_t i = 0; i < threads; i++)
    workers.emplace_back(std::thread(append_worker, i));
  for (auto &worker : workers)
    worker.join();
  if (failed) {
    for (auto &c : clones) {
      c->abort();
      c->end();
    }
    warren->abort();
    return finish(false);
  }
  std::vector<std::shared_ptr<Warren>> warrens = clones;
  warrens.push_back(warren);
  Warren::commit_all(warrens);
  for (auto &c : clones)
    c->end();
  return finish(true);
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
  params.erase("gcl");
  params["contents"] = gcl;
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
