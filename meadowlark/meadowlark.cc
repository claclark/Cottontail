#include "meadowlark/meadowlark.h"

#include <fnmatch.h>

#include <algorithm>
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

namespace {
const std::string default_recipe =
    "tokenizer:name:utf8 featurizer@json idx:fvalue_compressor:zlib "
    "idx:posting_compressor:post txt:compressor:zlib ";
}

std::shared_ptr<Warren> create_meadow(const std::string &meadow,
                                      const std::string &recipe,
                                      std::string *error) {
  std::string the_meadow = meadow == "" ? DEFAULT_MEADOW : meadow;
  std::shared_ptr<Warren> bigwig =
      Bigwig::make(the_meadow, default_recipe + recipe, error);
  if (bigwig == nullptr ||
      !bigwig->set_parameter("format", "meadowlark", error))
    return nullptr;
  else
    return bigwig;
}

std::shared_ptr<Warren> create_meadow(const std::string &meadow,
                                      std::string *error) {
  return create_meadow(meadow, "", error);
}

std::shared_ptr<Warren> create_meadow(std::string *error) {
  return create_meadow("", "", error);
}

std::shared_ptr<Warren> open_meadow(const std::string &meadow,
                                    std::string *error) {
  std::string the_meadow = meadow == "" ? DEFAULT_MEADOW : meadow;
  std::shared_ptr<Warren> warren = Warren::make(the_meadow, error);
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
  return open_meadow("", error);
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
  size_t end = text.size();
  while (end > start &&
         std::isspace(static_cast<unsigned char>(text[end - 1])))
    end--;
  if (end - start >= 2 && text[start] == '"' && text[end - 1] == '"') {
    start++;
    end--;
  }
  return text.compare(start, end - start, path) == 0 &&
         end - start == path.size();
}

std::string framed_path(const std::string &filename) {
  return open_string_token + normalized_path(filename) + close_string_token;
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
  if (!appender->append(framed_path(filename), p, q, error) ||
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
  if (!appender->append(framed_path(filename), p, q, error) ||
      !annotator->annotate(featurizer->featurize("/"), *p, *q, error))
    return false;
  *path_feature = featurizer->featurize(normalized_path(filename));
  return true;
}

bool append_source_metadata(std::shared_ptr<Warren> warren,
                            const std::string &filename,
                            const std::string &metadata, addr *path_feature,
                            std::string *error) {
  assert(warren != nullptr);
  assert(path_feature != nullptr);
  addr source_p, source_q, metadata_p, metadata_q;
  return append_path(warren->appender(), warren->annotator(),
                     warren->featurizer(), filename, path_feature, &source_p,
                     &source_q, error) &&
         json_append(metadata, warren, &metadata_p, &metadata_q, "@", error);
}

bool append_source(std::shared_ptr<Warren> warren,
                   const std::string &filename, const std::string &metadata,
                   bool append_local_name, addr *path_feature,
                   std::string *error) {
  assert(warren != nullptr);
  assert(path_feature != nullptr);
  if (!warren->transaction(error))
    return false;
  addr segment_p, segment_q;
  if (!append_source_metadata(warren, filename, metadata, path_feature, error) ||
      (append_local_name &&
       !append_segment_name(warren->appender(), warren->annotator(),
                            warren->featurizer(), filename, &segment_p,
                            &segment_q, error)) ||
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
    if (path_match(json_translate(warren->txt()->translate(p, q)), path)) {
      *appended = true;
      return true;
    }
  }
  return true;
}

bool append_text(std::shared_ptr<Warren> warren, const std::string &filename,
                 std::string *error) {
  assert(warren != nullptr);
  warren->start();
  auto finish = [&](bool result) {
    warren->end();
    return result;
  };
  std::shared_ptr<std::string> contents = inhale(filename, error);
  if (contents == nullptr || !warren->transaction(error))
    return finish(false);
  addr path_feature;
  addr segment_p, segment_q;
  addr contents_p, contents_q;
  if (!append_source_metadata(warren, filename,
                              text_metadata(normalized_path(filename)),
                              &path_feature, error) ||
      !append_segment_name(warren->appender(), warren->annotator(),
                           warren->featurizer(), filename, &segment_p,
                           &segment_q, error) ||
      !warren->appender()->append(*contents, &contents_p, &contents_q, error)) {
    warren->abort();
    return finish(false);
  }
  if (contents_p <= contents_q) {
    if (!warren->annotator()->annotate(warren->featurizer()->featurize(":"),
                                       contents_p, contents_q, error) ||
        !warren->annotator()->annotate(path_feature, contents_p, contents_q,
                                       error)) {
      warren->abort();
      return finish(false);
    }
    segment_q = std::max(segment_q, contents_q);
  }
  if ((contents_p <= contents_q &&
       !warren->annotator()->annotate(warren->featurizer()->featurize("/."),
                                      segment_p, segment_q, error)) ||
      !warren->ready(error)) {
    warren->abort();
    return finish(false);
  }
  warren->commit();
  return finish(true);
}

bool append_code(std::shared_ptr<Warren> warren, const std::string &filename,
                 std::string *error) {
  assert(warren != nullptr);
  warren->start();
  auto finish = [&](bool result) {
    warren->end();
    return result;
  };
  std::unique_ptr<std::istream> input = maybe_zipped(filename, error);
  if (input == nullptr || !warren->transaction(error))
    return finish(false);
  addr path_feature;
  addr segment_p, segment_q;
  if (!append_source_metadata(warren, filename,
                              code_metadata(normalized_path(filename)),
                              &path_feature, error) ||
      !append_segment_name(warren->appender(), warren->annotator(),
                           warren->featurizer(), filename, &segment_p,
                           &segment_q, error)) {
    warren->abort();
    return finish(false);
  }
  addr line_feature = warren->featurizer()->featurize("#");
  addr contents_p = maxfinity, contents_q = minfinity;
  addr line_number = 1;
  std::string line;
  while (std::getline(*input, line)) {
    addr p, q;
    line += '\n';
    if (!warren->appender()->append(line, &p, &q, error) ||
        (p <= q &&
         !warren->annotator()->annotate(line_feature, p, q, line_number,
                                        error))) {
      warren->abort();
      return finish(false);
    }
    if (p <= q) {
      contents_p = std::min(contents_p, p);
      contents_q = std::max(contents_q, q);
      segment_q = std::max(segment_q, q);
    }
    line_number++;
  }
  if (!input->eof()) {
    safe_error(error) = "Read error on: " + filename;
    warren->abort();
    return finish(false);
  }
  if ((contents_p <= contents_q &&
       (!warren->annotator()->annotate(warren->featurizer()->featurize(":"),
                                       contents_p, contents_q, error) ||
        !warren->annotator()->annotate(path_feature, contents_p, contents_q,
                                       error) ||
        !warren->annotator()->annotate(
            warren->featurizer()->featurize("/."), segment_p, segment_q,
            error))) ||
      !warren->ready(error)) {
    warren->abort();
    return finish(false);
  }
  warren->commit();
  return finish(true);
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
  std::string first_line;
  bool have_first_line = static_cast<bool>(std::getline(*input, first_line));
  if (!have_first_line && !input->eof()) {
    safe_error(error) = "Read error on: " + filename;
    return finish(false);
  }
  addr path_feature;
  if (!append_source(warren, filename,
                     json_metadata(normalized_path(filename)),
                     !have_first_line, &path_feature, error))
    return finish(false);
  if (!have_first_line) {
    Warren::commit_all({warren});
    return finish(true);
  }
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
  auto append_worker = [&](size_t n) {
    std::string terror;
    std::shared_ptr<Warren> twarren = clones[n];
    addr data_p = maxfinity, data_q = minfinity;
    addr segment_p = maxfinity, segment_q = minfinity;
    for (;;) {
      std::string line;
      {
        std::lock_guard<std::mutex> _(sync);
        if (done)
          break;
        if (first_pending) {
          line = first_line;
          first_pending = false;
        } else if (!std::getline(*input, line)) {
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
      if (!json_append(line, twarren, &p, &q, ":", &terror)) {
        std::lock_guard<std::mutex> _(sync);
        if (!failed) {
          done = failed = true;
          safe_set(error) = terror;
        }
        return;
      }
      data_p = std::min(data_p, p);
      data_q = std::max(data_q, q);
      segment_q = std::max(segment_q, q);
    }
    if ((data_p <= data_q &&
         (!twarren->annotator()->annotate(path_feature, data_p, data_q,
                                          &terror) ||
          !twarren->annotator()->annotate(
              twarren->featurizer()->featurize("/."), segment_p, segment_q,
              &terror))) ||
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
                     !have_first_line, &path_feature, error))
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
         (!twarren->annotator()->annotate(path_feature, p, q, &terror) ||
          !twarren->annotator()->annotate(
              twarren->featurizer()->featurize("/."), segment_p, segment_q,
              &terror))) ||
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

namespace {
bool valid_input_type(InputType type) {
  return type == InputType::TSV || type == InputType::JSONL ||
         type == InputType::TEXT || type == InputType::CODE;
}

bool text_or_code(InputType type) {
  return type == InputType::TEXT || type == InputType::CODE;
}

bool append_one(std::shared_ptr<Warren> warren, const InputFile &file,
                std::string *error, size_t threads) {
  switch (file.type) {
  case InputType::TSV:
    return append_tsv(warren, file.filename, error, false, "\t", threads);
  case InputType::JSONL:
    return append_jsonl(warren, file.filename, error, threads, false);
  case InputType::TEXT:
    return append_text(warren, file.filename, error);
  case InputType::CODE:
    return append_code(warren, file.filename, error);
  default:
    safe_error(error) = "No input type for: " + file.filename;
    return false;
  }
}
} // namespace

bool append_all(std::shared_ptr<Warren> warren,
                const std::vector<InputFile> &files, std::string *error,
                size_t threads, bool verbose) {
  assert(warren != nullptr);
  std::vector<bool> done(files.size(), false);
  for (const auto &file : files)
    if (!valid_input_type(file.type)) {
      safe_error(error) = "No input type for: " + file.filename;
      return false;
    }

  warren->start();
  for (size_t i = 0; i < files.size(); i++) {
    bool appended;
    if (!already_appended(warren, files[i].filename, &appended, error)) {
      warren->end();
      return false;
    }
    done[i] = appended;
    if (done[i] && verbose)
      std::cerr << "Skipping existing file " << files[i].filename << "\n"
                << std::flush;
  }
  warren->end();

  for (size_t i = 0; i < files.size(); i++) {
    if (done[i])
      continue;
    if (!text_or_code(files[i].type)) {
      if (verbose)
        std::cerr << "Appending " << files[i].filename << "\n" << std::flush;
      if (!append_one(warren, files[i], error, threads))
        return false;
      done[i] = true;
      continue;
    }

    std::vector<size_t> work;
    for (size_t j = i; j < files.size(); j++)
      if (!done[j] && text_or_code(files[j].type)) {
        work.push_back(j);
        done[j] = true;
      }
    size_t workers = std::min(work.size(), allowed_threads(threads));
    std::vector<std::shared_ptr<Warren>> clones;
    for (size_t j = 0; j < workers; j++) {
      std::shared_ptr<Warren> clone = warren->clone(error);
      if (clone == nullptr) {
        for (auto &c : clones)
          c->end();
        return false;
      }
      clones.push_back(clone);
    }

    bool failed = false;
    size_t next = 0;
    std::mutex sync;
    auto worker = [&](size_t n) {
      for (;;) {
        size_t j;
        {
          std::lock_guard<std::mutex> _(sync);
          if (failed || next == work.size())
            break;
          j = work[next++];
          if (verbose)
            std::cerr << "Appending " << files[j].filename << "\n"
                      << std::flush;
        }
        std::string terror;
        if (!append_one(clones[n], files[j], &terror, 1)) {
          std::lock_guard<std::mutex> _(sync);
          if (!failed) {
            failed = true;
            safe_set(error) = terror;
          }
          break;
        }
      }
      clones[n]->end();
    };
    std::vector<std::thread> pool;
    for (size_t j = 0; j < workers; j++)
      pool.emplace_back(std::thread(worker, j));
    for (auto &thread : pool)
      thread.join();
    if (failed)
      return false;
  }
  return true;
}

namespace {
std::string normalized_forager_name(const std::string &name) {
  return name == "" ? "tf-idf" : name;
}

std::string normalized_forager_tag(const std::string &tag) {
  return tag == "" ? "none" : tag;
}

std::string forager_named_query(const std::string &name) {
  std::string typed = "(>> @ (>> :type: \"forager\"))";
  return "(>> " + typed + " (>> :name: " + gcl_string(name) + "))";
}

std::string forager_tagged_query(const std::string &name,
                                 const std::string &tag) {
  return "(>> " + forager_named_query(name) + " (>> :tag: " +
         gcl_string(tag) + "))";
}

struct Definition {
  bool current = false;
  bool legacy = false;
  bool completion = false;
  std::string query;
  std::map<std::string, std::string> parameters;
};

bool lookup_definition(std::shared_ptr<Warren> warren, const std::string &name,
                       const std::string &tag, Definition *definition,
                       std::string *error) {
  assert(warren != nullptr);
  assert(definition != nullptr);
  *definition = Definition();
  std::unique_ptr<Hopper> hopper =
      warren->hopper_from_gcl(forager_named_query(name), error);
  if (hopper == nullptr)
    return false;
  addr p, q;
  for (hopper->tau(minfinity + 1, &p, &q); p < maxfinity;
       hopper->tau(p + 1, &p, &q)) {
    ForagerMetadata metadata;
    if (!json2forager(warren->txt()->translate(p, q), &metadata, error))
      return false;
    if (normalized_forager_tag(metadata.tag) != tag)
      continue;
    if (metadata.has_filename) {
      definition->completion = true;
      continue;
    }
    if (!metadata.has_query) {
      definition->legacy = true;
      continue;
    }
    if (definition->current &&
        (definition->query != metadata.query ||
         definition->parameters != metadata.parameters)) {
      safe_error(error) = "Conflicting forager definitions for: " + name +
                          ":" + tag;
      return false;
    }
    definition->current = true;
    definition->query = metadata.query;
    definition->parameters = metadata.parameters;
  }
  if (name == "tf-idf" && tag == "none") {
    hopper = warren->idx()->hopper(warren->featurizer()->featurize("@tf-idf:"));
    if (hopper != nullptr) {
      hopper->tau(minfinity + 1, &p, &q);
      if (p < maxfinity)
        definition->legacy = true;
    }
  }
  return true;
}

bool legacy_error(const std::string &name, const std::string &tag,
                  std::string *error) {
  safe_error(error) = "The existing " + name + ":" + tag +
                      " layer uses historical interval metadata; it remains "
                      "readable but cannot be extended. Use a new tag.";
  return false;
}

bool write_definition(std::shared_ptr<Warren> warren, const std::string &name,
                      const std::string &tag, const std::string &query,
                      const std::map<std::string, std::string> &parameters,
                      std::string *error) {
  if (!warren->transaction(error))
    return false;
  addr p, q;
  if (!json_append(forager2json(name, tag, query, parameters), warren, &p, &q,
                   "@", error) ||
      !warren->ready(error)) {
    warren->abort();
    return false;
  }
  warren->commit();
  return true;
}

bool resolve_definition(
    std::shared_ptr<Warren> warren, const std::string &query,
    const std::string &name, const std::string &tag,
    const std::map<std::string, std::string> *supplied,
    std::map<std::string, std::string> *resolved, std::string *error) {
  assert(resolved != nullptr);
  Definition definition;
  if (!lookup_definition(warren, name, tag, &definition, error))
    return false;
  if (definition.legacy)
    return legacy_error(name, tag, error);
  if (supplied == nullptr) {
    if (!definition.current) {
      safe_error(error) = "No current forager definition for: " + name + ":" +
                          tag;
      return false;
    }
    if (definition.query != query) {
      safe_error(error) = "Forager query does not match definition for: " +
                          name + ":" + tag;
      return false;
    }
    *resolved = definition.parameters;
    if (warren->hopper_from_gcl(query, error) == nullptr)
      return false;
    return Forager::check(warren, name, tag, *resolved, error);
  }
  if (warren->hopper_from_gcl(query, error) == nullptr)
    return false;
  if (!Forager::check(warren, name, tag, *supplied, error))
    return false;
  if (definition.current) {
    if (definition.query != query || definition.parameters != *supplied) {
      safe_error(error) = "Forager specification does not match definition "
                          "for: " +
                          name + ":" + tag;
      return false;
    }
  } else {
    if (definition.completion) {
      safe_error(error) = "Forager completion exists without definition for: " +
                          name + ":" + tag;
      return false;
    }
    if (!write_definition(warren, name, tag, query, *supplied, error))
      return false;
  }
  *resolved = *supplied;
  return true;
}

bool decode_path(const std::string &text, std::string *path) {
  assert(path != nullptr);
  size_t start = 0;
  while (start < text.size() &&
         std::isspace(static_cast<unsigned char>(text[start])))
    start++;
  size_t end = text.size();
  while (end > start &&
         std::isspace(static_cast<unsigned char>(text[end - 1])))
    end--;
  if (end - start >= 2 && text[start] == '"' && text[end - 1] == '"') {
    start++;
    end--;
  }
  if (start == end)
    return false;
  *path = normalized_path(text.substr(start, end - start));
  return true;
}

bool meadow_inventory(std::shared_ptr<Warren> warren,
                      std::vector<std::string> *paths, std::string *error) {
  assert(paths != nullptr);
  paths->clear();
  std::set<std::string> unique;
  std::unique_ptr<Hopper> hopper =
      warren->idx()->hopper(warren->featurizer()->featurize("/"));
  if (hopper == nullptr) {
    safe_error(error) = "Cannot read meadow file inventory";
    return false;
  }
  addr p, q;
  for (hopper->tau(minfinity + 1, &p, &q); p < maxfinity;
       hopper->tau(p + 1, &p, &q)) {
    std::string path;
    if (!decode_path(json_translate(warren->txt()->translate(p, q)), &path)) {
      safe_error(error) = "Cannot decode meadow filename";
      return false;
    }
    unique.insert(path);
  }
  paths->assign(unique.begin(), unique.end());
  return true;
}

bool expand_selectors(std::shared_ptr<Warren> warren,
                      const std::vector<std::string> &selectors,
                      std::vector<std::string> *paths, std::string *error) {
  std::vector<std::string> inventory;
  if (!meadow_inventory(warren, &inventory, error))
    return false;
  if (selectors.empty()) {
    *paths = inventory;
    return true;
  }
  std::set<std::string> selected;
  for (const auto &selector : selectors) {
    std::string pattern = normalized_path(selector);
    bool matched = false;
    for (const auto &path : inventory)
      if (fnmatch(pattern.c_str(), path.c_str(), FNM_PATHNAME) == 0) {
        selected.insert(path);
        matched = true;
      }
    if (!matched) {
      safe_error(error) = "No meadow file matches: " + selector;
      return false;
    }
  }
  paths->assign(selected.begin(), selected.end());
  return true;
}

bool scoped_intervals(std::shared_ptr<Warren> warren,
                      const std::string &filename, const std::string &query,
                      std::vector<std::pair<addr, addr>> *intervals,
                      std::string *error) {
  assert(intervals != nullptr);
  intervals->clear();
  std::string scoped = "(<< " + query + " " + filename + ")";
  std::unique_ptr<Hopper> hopper = warren->hopper_from_gcl(scoped, error);
  if (hopper == nullptr)
    return false;
  addr p, q;
  for (hopper->tau(minfinity + 1, &p, &q); p < maxfinity;
       hopper->tau(p + 1, &p, &q))
    intervals->emplace_back(p, q);
  return true;
}

bool source_type(std::shared_ptr<Warren> warren, const std::string &filename,
                 InputType *input_type, std::string *error) {
  assert(input_type != nullptr);
  *input_type = InputType::NONE;
  std::vector<std::string> fields = {"filename", "file"};
  for (const auto &field : fields) {
    std::string query = "(>> @ (>> :" + field + ": " +
                        gcl_string(filename) + "))";
    std::unique_ptr<Hopper> hopper = warren->hopper_from_gcl(query, error);
    if (hopper == nullptr)
      return false;
    addr p, q;
    for (hopper->tau(minfinity + 1, &p, &q); p < maxfinity;
         hopper->tau(p + 1, &p, &q)) {
      std::string type, path;
      if (!json2file(warren->txt()->translate(p, q), &type, &path, error))
        return false;
      if (normalized_path(path) != filename || type == "forager")
        continue;
      InputType found = InputType::NONE;
      if (type == "tsv")
        found = InputType::TSV;
      else if (type == "json")
        found = InputType::JSONL;
      else if (type == "text")
        found = InputType::TEXT;
      else if (type == "code")
        found = InputType::CODE;
      if (found == InputType::NONE) {
        safe_error(error) = "Unknown source type for: " + filename;
        return false;
      }
      if (*input_type != InputType::NONE && *input_type != found) {
        safe_error(error) = "Conflicting source metadata for: " + filename;
        return false;
      }
      *input_type = found;
    }
  }
  if (*input_type == InputType::NONE) {
    safe_error(error) = "No source metadata for: " + filename;
    return false;
  }
  return true;
}
} // namespace

bool already_foraged(std::shared_ptr<Warren> warren,
                     const std::string &filename, const std::string &name,
                     const std::string &tag, bool *foraged,
                     std::string *error) {
  assert(warren != nullptr);
  assert(foraged != nullptr);
  *foraged = false;
  std::string path = normalized_path(filename);
  std::string n = normalized_forager_name(name);
  std::string t = normalized_forager_tag(tag);
  std::string query = "(>> " + forager_tagged_query(n, t) +
                      " (>> :filename: " + gcl_string(path) + "))";
  std::unique_ptr<Hopper> hopper = warren->hopper_from_gcl(query, error);
  if (hopper == nullptr)
    return false;
  addr p, q;
  for (hopper->tau(minfinity + 1, &p, &q); p < maxfinity;
       hopper->tau(p + 1, &p, &q)) {
    ForagerMetadata metadata;
    if (!json2forager(warren->txt()->translate(p, q), &metadata, error))
      return false;
    if (metadata.has_filename && normalized_path(metadata.filename) == path &&
        metadata.name == n && normalized_forager_tag(metadata.tag) == t) {
      *foraged = true;
      return true;
    }
  }
  return true;
}

namespace {
struct ForageWorker {
  std::shared_ptr<Warren> warren;
  std::shared_ptr<Forager> forager;
  size_t start;
  size_t end;
};

bool forage_file_started(
    std::shared_ptr<Warren> warren, const std::string &filename,
    const std::vector<std::pair<addr, addr>> &intervals,
    const std::string &name, const std::string &tag,
    const std::map<std::string, std::string> &parameters, std::string *error,
    size_t threads) {
  size_t worker_count =
      intervals.empty()
          ? 0
          : std::min(intervals.size(), thread_count(threads, intervals.size()));
  std::vector<ForageWorker> workers;
  std::vector<std::shared_ptr<Warren>> transactions;
  auto abort_all = [&]() {
    for (auto &transaction : transactions) {
      transaction->abort();
      transaction->end();
    }
  };
  size_t start = 0;
  for (size_t i = 0; i < worker_count; i++) {
    std::shared_ptr<Warren> clone = warren->clone(error);
    if (clone == nullptr) {
      abort_all();
      return false;
    }
    if (!clone->transaction(error)) {
      clone->end();
      abort_all();
      return false;
    }
    transactions.push_back(clone);
    std::shared_ptr<Forager> forager =
        Forager::make(clone, name, tag, parameters, error);
    if (forager == nullptr) {
      abort_all();
      return false;
    }
    size_t count = intervals.size() / worker_count;
    if (i < intervals.size() % worker_count)
      count++;
    workers.push_back({clone, forager, start, start + count});
    start += count;
  }

  std::shared_ptr<Warren> marker = warren->clone(error);
  if (marker == nullptr) {
    abort_all();
    return false;
  }
  if (!marker->transaction(error)) {
    marker->end();
    abort_all();
    return false;
  }
  transactions.push_back(marker);
  addr marker_p, marker_q;
  if (!json_append(forager_file2json(filename, name, tag), marker, &marker_p,
                   &marker_q, "@", error)) {
    abort_all();
    return false;
  }

  std::vector<std::string> worker_errors(worker_count);
  std::vector<char> worker_ok(worker_count, true);
  std::vector<std::thread> pool;
  for (size_t i = 0; i < worker_count; i++)
    pool.emplace_back([&, i]() {
      for (size_t j = workers[i].start; j < workers[i].end; j++)
        if (!workers[i].forager->forage(intervals[j].first,
                                        intervals[j].second,
                                        &worker_errors[i])) {
          worker_ok[i] = false;
          return;
        }
      worker_ok[i] = workers[i].forager->finish(&worker_errors[i]) &&
                     workers[i].warren->ready(&worker_errors[i]);
    });
  for (auto &worker : pool)
    worker.join();
  for (size_t i = 0; i < worker_count; i++)
    if (!worker_ok[i]) {
      safe_set(error) = worker_errors[i];
      abort_all();
      return false;
    }
  if (!marker->ready(error)) {
    abort_all();
    return false;
  }
  Warren::commit_all(transactions);
  for (auto &transaction : transactions)
    transaction->end();
  return true;
}

struct ForageWork {
  std::string filename;
  InputType type;
  std::vector<std::pair<addr, addr>> intervals;
};

bool prepare_work(std::shared_ptr<Warren> warren,
                  const std::vector<std::string> &selectors,
                  const std::string &query, const std::string &name,
                  const std::string &tag, std::vector<ForageWork> *work,
                  std::string *error) {
  assert(work != nullptr);
  work->clear();
  std::vector<std::string> paths;
  if (!expand_selectors(warren, selectors, &paths, error))
    return false;
  for (const auto &path : paths) {
    std::vector<std::pair<addr, addr>> intervals;
    if (selectors.empty()) {
      if (!scoped_intervals(warren, path, query, &intervals, error))
        return false;
      if (intervals.empty())
        continue;
    }
    bool done;
    if (!already_foraged(warren, path, name, tag, &done, error))
      return false;
    if (done)
      continue;
    if (!selectors.empty() &&
        !scoped_intervals(warren, path, query, &intervals, error))
      return false;
    InputType type;
    if (!source_type(warren, path, &type, error))
      return false;
    work->push_back({path, type, std::move(intervals)});
  }
  return true;
}

bool execute_work(std::shared_ptr<Warren> warren,
                  const std::vector<ForageWork> &work,
                  const std::string &name, const std::string &tag,
                  const std::map<std::string, std::string> &parameters,
                  std::string *error, size_t threads) {
  std::vector<bool> done(work.size(), false);
  for (size_t i = 0; i < work.size(); i++) {
    if (done[i])
      continue;
    if (!text_or_code(work[i].type)) {
      if (!forage_file_started(warren, work[i].filename, work[i].intervals,
                               name, tag, parameters, error, threads))
        return false;
      done[i] = true;
      continue;
    }
    std::vector<size_t> group;
    for (size_t j = i; j < work.size(); j++)
      if (!done[j] && text_or_code(work[j].type)) {
        done[j] = true;
        group.push_back(j);
      }
    size_t worker_count =
        std::min(group.size(), std::max<size_t>(1, allowed_threads(threads)));
    bool failed = false;
    size_t next = 0;
    std::mutex sync;
    auto worker = [&]() {
      for (;;) {
        size_t j;
        {
          std::lock_guard<std::mutex> _(sync);
          if (failed || next == group.size())
            return;
          j = group[next++];
        }
        std::string terror;
        if (!forage_file_started(warren, work[j].filename, work[j].intervals,
                                 name, tag, parameters, &terror, 1)) {
          std::lock_guard<std::mutex> _(sync);
          if (!failed) {
            failed = true;
            safe_set(error) = terror;
          }
          return;
        }
      }
    };
    std::vector<std::thread> pool;
    for (size_t j = 0; j < worker_count; j++)
      pool.emplace_back(worker);
    for (auto &thread : pool)
      thread.join();
    if (failed)
      return false;
  }
  return true;
}

bool forage_one_impl(
    std::shared_ptr<Warren> warren, const std::string &filename,
    const std::string &query, const std::string &name, const std::string &tag,
    const std::map<std::string, std::string> *supplied, std::string *error,
    size_t threads) {
  std::string path = normalized_path(filename);
  std::string n = normalized_forager_name(name);
  std::string t = normalized_forager_tag(tag);
  std::map<std::string, std::string> parameters;
  if (!resolve_definition(warren, query, n, t, supplied, &parameters, error))
    return false;
  bool appended;
  if (!already_appended(warren, path, &appended, error))
    return false;
  if (!appended) {
    safe_error(error) = "No meadow file named: " + path;
    return false;
  }
  bool done;
  if (!already_foraged(warren, path, n, t, &done, error))
    return false;
  if (done)
    return true;
  std::vector<std::pair<addr, addr>> intervals;
  if (!scoped_intervals(warren, path, query, &intervals, error))
    return false;
  return forage_file_started(warren, path, intervals, n, t, parameters, error,
                             threads);
}

bool forage_all_impl(
    std::shared_ptr<Warren> warren,
    const std::vector<std::string> &selectors, const std::string &query,
    const std::string &name, const std::string &tag,
    const std::map<std::string, std::string> *supplied, std::string *error,
    size_t threads) {
  std::string n = normalized_forager_name(name);
  std::string t = normalized_forager_tag(tag);
  std::map<std::string, std::string> parameters;
  if (!resolve_definition(warren, query, n, t, supplied, &parameters, error))
    return false;
  std::vector<ForageWork> work;
  if (!prepare_work(warren, selectors, query, n, t, &work, error))
    return false;
  return execute_work(warren, work, n, t, parameters, error, threads);
}
} // namespace

bool forage(std::shared_ptr<Warren> warren, const std::string &filename,
            const std::string &query, const std::string &name,
            const std::string &tag,
            const std::map<std::string, std::string> &parameters,
            std::string *error, size_t threads) {
  assert(warren != nullptr);
  warren->start();
  bool result = forage_one_impl(warren, filename, query, name, tag,
                                &parameters, error, threads);
  warren->end();
  return result;
}

bool forage(std::shared_ptr<Warren> warren, const std::string &filename,
            const std::string &query, const std::string &name,
            const std::string &tag, std::string *error, size_t threads) {
  assert(warren != nullptr);
  warren->start();
  bool result = forage_one_impl(warren, filename, query, name, tag, nullptr,
                                error, threads);
  warren->end();
  return result;
}

bool forage_all(std::shared_ptr<Warren> warren,
                const std::vector<std::string> &filenames,
                const std::string &query, const std::string &name,
                const std::string &tag,
                const std::map<std::string, std::string> &parameters,
                std::string *error, size_t threads) {
  assert(warren != nullptr);
  warren->start();
  bool result = forage_all_impl(warren, filenames, query, name, tag,
                                &parameters, error, threads);
  warren->end();
  return result;
}

bool forage_all(std::shared_ptr<Warren> warren,
                const std::vector<std::string> &filenames,
                const std::string &query, const std::string &name,
                const std::string &tag, std::string *error, size_t threads) {
  assert(warren != nullptr);
  warren->start();
  bool result = forage_all_impl(warren, filenames, query, name, tag, nullptr,
                                error, threads);
  warren->end();
  return result;
}
} // namespace meadowlark
} // namespace cottontail
