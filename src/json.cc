#include "src/json.h"

#include <cassert>
#include <memory>
#include <string>

#include "src/core.h"
#include "src/json.h"
#include "src/scribe.h"

namespace cottontail {

namespace {
bool do_json(json &j, std::shared_ptr<Scribe> scribe, const std::string &path,
             const std::string &label, addr *p, addr *q, std::string *error);

bool do_null(json &j, std::shared_ptr<Scribe> scribe, const std::string &path,
             const std::string &label, addr *p, addr *q, std::string *error) {
  if (!scribe->appender()->append("null", p, q, error))
    return false;
  if (label != "" &&
      !scribe->annotator()->annotate(scribe->featurizer()->featurize(label), *p,
                                     *q, NAN, error))
    return false;
  return scribe->annotator()->annotate(scribe->featurizer()->featurize(path),
                                       *p, *q, NAN, error);
}

bool do_boolean(json &j, std::shared_ptr<Scribe> scribe,
                const std::string &path, const std::string &label, addr *p,
                addr *q, std::string *error) {
  if (j) {
    if (!scribe->appender()->append("true", p, q, error))
      return false;
    if (label != "" &&
        !scribe->annotator()->annotate(scribe->featurizer()->featurize(label),
                                       *p, *q, 1.0, error))
      return false;
    return scribe->annotator()->annotate(scribe->featurizer()->featurize(path),
                                         *p, *q, 1.0, error);
  } else {
    if (!scribe->appender()->append("false", p, q, error))
      return false;
    if (label != "" &&
        !scribe->annotator()->annotate(scribe->featurizer()->featurize(label),
                                       *p, *q, 0.0, error))
      return false;
    return scribe->annotator()->annotate(scribe->featurizer()->featurize(path),
                                         *p, *q, 0.0, error);
  }
  return true;
}

bool do_number(json &j, std::shared_ptr<Scribe> scribe, const std::string &path,
               const std::string &label, addr *p, addr *q, std::string *error) {
  fval v = j;
  std::string s = open_number_token + std::to_string(v) + close_number_token;
  if (!scribe->appender()->append(s, p, q, error))
    return false;
  if (label != "" &&
      !scribe->annotator()->annotate(scribe->featurizer()->featurize(label), *p,
                                     *q, v, error))
    return false;
  return scribe->annotator()->annotate(scribe->featurizer()->featurize(path),
                                       *p, *q, v, error);
}

bool do_string(json &j, std::shared_ptr<Scribe> scribe, const std::string &path,
               const std::string &label, addr *p, addr *q, std::string *error) {
  std::string s = open_string_token + (std::string)j + close_string_token;
  if (!scribe->appender()->append(s, p, q, error))
    return false;
  if (label != "" &&
      !scribe->annotator()->annotate(scribe->featurizer()->featurize(label), *p,
                                     *q, 0.0, error))
    return false;
  return scribe->annotator()->annotate(scribe->featurizer()->featurize(path),
                                       *p, *q, 0.0, error);
}

bool do_array(json &j, std::shared_ptr<Scribe> scribe, const std::string &path,
              const std::string &label, addr *p, addr *q, std::string *error) {
  addr p0, q0;
  if (!scribe->appender()->append(open_array_token, p, &q0, error))
    return false;
  size_t index = 0;
  for (json::iterator it = j.begin(); it != j.end(); it++) {
    if (it != j.begin()) {
      if (!scribe->appender()->append(comma_token, &p0, &q0, error))
        return false;
    }
    std::string element = "[" + std::to_string(index) + "]:";
    if (!do_json(it.value(), scribe, path + element, label + element, &p0, &q0,
                 error))
      return false;
    index++;
  }
  if (!scribe->appender()->append(close_array_token, &p0, q, error))
    return false;
  if (label != "" &&
      !scribe->annotator()->annotate(scribe->featurizer()->featurize(label), *p,
                                     *q, (fval)j.size(), error))
    return false;
  return scribe->annotator()->annotate(scribe->featurizer()->featurize(path),
                                       *p, *q, (fval)j.size(), error);
}

bool do_object(json &j, std::shared_ptr<Scribe> scribe, const std::string &path,
               const std::string &label, addr *p, addr *q, std::string *error) {
  addr p0, q0;
  if (!scribe->appender()->append(open_object_token, p, &q0, error))
    return false;
  for (json::iterator it = j.begin(); it != j.end(); it++) {
    if (it != j.begin()) {
      if (!scribe->appender()->append(comma_token, &p0, &q0, error))
        return false;
    }
    std::string key =
        open_string_token + it.key() + close_string_token + colon_token;
    if (!scribe->appender()->append(key, &p0, &q0, error))
      return false;
    if (!do_json(it.value(), scribe, path + it.key() + ":", it.key() + ":", &p0,
                 &q0, error))
      return false;
  }
  if (!scribe->appender()->append(close_object_token, &p0, q, error))
    return false;
  if (label != "" &&
      !scribe->annotator()->annotate(scribe->featurizer()->featurize(label), *p,
                                     *q, 0.0, error))
    return false;
  return scribe->annotator()->annotate(scribe->featurizer()->featurize(path),
                                       *p, *q, 0.0, error);
}

bool do_json(json &j, std::shared_ptr<Scribe> scribe, const std::string &path,
             const std::string &label, addr *p, addr *q, std::string *error) {
  if (j.is_null())
    return do_null(j, scribe, path, label, p, q, error);
  else if (j.is_boolean())
    return do_boolean(j, scribe, path, label, p, q, error);
  else if (j.is_number())
    return do_number(j, scribe, path, label, p, q, error);
  else if (j.is_string())
    return do_string(j, scribe, path, label, p, q, error);
  else if (j.is_array())
    return do_array(j, scribe, path, label, p, q, error);
  else if (j.is_object())
    return do_object(j, scribe, path, label, p, q, error);
  safe_set(error) = "Unknown JSON data type.";
  return false;
}
} // namespace

bool json_scribe(json &j, std::shared_ptr<Scribe> scribe, addr *p, addr *q,
                 std::string *error) {
  if (scribe == nullptr) {
    safe_set(error) = "Null scribe";
    return false;
  }
  return do_json(j, scribe, ":", "", p, q, error);
}

bool json_scribe(json &j, std::shared_ptr<Scribe> scribe, std::string *error) {
  addr p, q;
  return json_scribe(j, scribe, &p, &q, error);
}

namespace {
inline void sanity_check() { assert(noncharacter_token_length == 3); }

inline bool is_next(const char *c, const std::string token) {
  const char *t = token.c_str();
  return c[0] && c[0] == t[0] && c[1] && c[1] == t[1] && c[2] && c[2] == t[2];
}

inline const char *skip(const char *c) {
  return c + (noncharacter_token_length - 1);
}
} // namespace

std::string json_translate(const std::string &s) {
  sanity_check();
  bool inside = false;
  std::string t;
  for (const char *c = s.c_str(); *c; c++)
    if (is_next(c, open_object_token)) {
      t += "{";
      c = skip(c);
    } else if (is_next(c, close_object_token)) {
      t += "}";
      c = skip(c);
    } else if (is_next(c, open_array_token)) {
      t += "[";
      c = skip(c);
    } else if (is_next(c, close_array_token)) {
      t += "]";
      c = skip(c);
    } else if (is_next(c, open_string_token)) {
      t += "\"";
      c = skip(c);
      inside = true;
    } else if (is_next(c, close_string_token)) {
      t += "\"";
      c = skip(c);
      inside = false;
    } else if (is_next(c, colon_token)) {
      t += ":";
      c = skip(c);
    } else if (is_next(c, comma_token)) {
      t += ",";
      c = skip(c);
    } else if (is_next(c, open_number_token) ||
               is_next(c, close_number_token)) {
      c = skip(c);
    } else if (*c == '\n') {
      t += ' ';
    } else if (inside) {
      if (*c == '"')
        t += "\\\"";
      else if (*c == '\\')
        t += "\\\\";
      else
        t += *c;
    } else {
      t += *c;
    }
  return t;
}

} // namespace cottontail
