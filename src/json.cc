#include "src/json.h"

#include <cassert>
#include <memory>
#include <string>

#include "src/core.h"
#include "src/nlohmann.h"
#include "src/scribe.h"

namespace cottontail {

namespace {
bool do_json(json &j, std::shared_ptr<Scribe> scribe, const std::string &path,
             addr *p, addr *q, std::string *error);

bool do_null(json &j, std::shared_ptr<Scribe> scribe, const std::string &path,
             addr *p, addr *q, std::string *error) {
  if (!scribe->appender()->append("null", p, q, error))
    return false;
  return scribe->annotator()->annotate(scribe->featurizer()->featurize(path),
                                       *p, *q, NAN, error);
}

bool do_boolean(json &j, std::shared_ptr<Scribe> scribe,
                const std::string &path, addr *p, addr *q, std::string *error) {
  if (j) {
    if (!scribe->appender()->append("true", p, q, error))
      return false;
    return scribe->annotator()->annotate(scribe->featurizer()->featurize(path),
                                         *p, *q, 1.0, error);
  } else {
    if (!scribe->appender()->append("false", p, q, error))
      return false;
    return scribe->annotator()->annotate(scribe->featurizer()->featurize(path),
                                         *p, *q, 0.0, error);
  }
  return true;
}

bool do_number(json &j, std::shared_ptr<Scribe> scribe, const std::string &path,
               addr *p, addr *q, std::string *error) {
  fval v = j;
  std::string s = open_number_token + std::to_string(v) + close_number_token;
  if (!scribe->appender()->append(s, p, q, error))
    return false;
  return scribe->annotator()->annotate(scribe->featurizer()->featurize(path),
                                       *p, *q, v, error);
}

bool do_string(json &j, std::shared_ptr<Scribe> scribe, const std::string &path,
               addr *p, addr *q, std::string *error) {
  std::string s = open_string_token + (std::string)j + close_string_token;
  if (!scribe->appender()->append(s, p, q, error))
    return false;
  return scribe->annotator()->annotate(scribe->featurizer()->featurize(path),
                                       *p, *q, 0.0, error);
}

bool do_array(json &j, std::shared_ptr<Scribe> scribe, const std::string &path,
              addr *p, addr *q, std::string *error) {
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
    if (!do_json(it.value(), scribe, path + element, &p0, &q0, error))
      return false;
    index++;
  }
  if (!scribe->appender()->append(close_array_token, &p0, q, error))
    return false;
  return scribe->annotator()->annotate(scribe->featurizer()->featurize(path),
                                       *p, *q, (fval)j.size(), error);
}

bool do_object(json &j, std::shared_ptr<Scribe> scribe, const std::string &path,
               addr *p, addr *q, std::string *error) {
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
    if (!do_json(it.value(), scribe, path + it.key() + ":", &p0, &q0, error))
      return false;
    if (!scribe->annotator()->annotate(
            scribe->featurizer()->featurize(it.key() + ":"), p0, q0, 0.0,
            error))
      return false;
  }
  if (!scribe->appender()->append(close_object_token, &p0, q, error))
    return false;
  return scribe->annotator()->annotate(scribe->featurizer()->featurize(path),
                                       *p, *q, 0.0, error);
}

bool do_json(json &j, std::shared_ptr<Scribe> scribe, const std::string &path,
             addr *p, addr *q, std::string *error) {
  if (j.is_null())
    return do_null(j, scribe, path, p, q, error);
  else if (j.is_boolean())
    return do_boolean(j, scribe, path, p, q, error);
  else if (j.is_number())
    return do_number(j, scribe, path, p, q, error);
  else if (j.is_string())
    return do_string(j, scribe, path, p, q, error);
  else if (j.is_array())
    return do_array(j, scribe, path, p, q, error);
  else if (j.is_object())
    return do_object(j, scribe, path, p, q, error);
  safe_set(error) = "Unknown JSON data type.";
  return false;
}

bool contains_utf8_noncharacters(const std::string &s) {
  int state = 0;
  for (const char *c = s.c_str(); *c; c++)
    if (state == 0) {
      if (*c == '\xEF')
        state = 1;
    } else if (state == 1) {
      if (*c == '\xB7')
        state = 2;
      else
        state = 0;
    } else if (*c >= '\x90' || *c <= '\xAF') {
      return true;
    } else {
      state = 0;
    }
  return false;
}

inline bool noncharacter_next(const char *c) {
  return c[0] && c[0] == '\xEF' && c[1] && c[1] == '\xB7' && c[2] &&
         (c[2] >= '\x90' || c[2] >= '\xAF');
}

std::string sanitize(const std::string &s) {
  std::string t;
  for (const char *c = s.c_str(); *c; c++)
    if (noncharacter_next(c)) {
      t += '\xEF'; // Unicode replacement character
      t += '\xBF';
      t += '\xBD';
      c += 2;
    } else {
      t += *c;
    }
  return t;
}
} // namespace

bool json_scribe(const std::string &s, std::shared_ptr<Scribe> scribe, addr *p,
                 addr *q, std::string *error) {
  assert(scribe != nullptr);
  json j;
  try {
    if (contains_utf8_noncharacters(s))
      j = json::parse(sanitize(s));
    else
      j = json::parse(s);
  } catch (json::parse_error &e) {
    safe_set(error) = "Cannot parse json";
    return false;
  }
  return do_json(j, scribe, ":", p, q, error);
}

bool json_scribe(const std::string &s, std::shared_ptr<Scribe> scribe,
                 std::string *error) {
  addr p, q;
  return json_scribe(s, scribe, &p, &q, error);
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
  bool pending_space = false;
  std::string t;
  for (const char *c = s.c_str(); *c; c++) {
    if (*c != ' ' && *c != '\n' && pending_space) {
      pending_space = false;
      t += ' ';
    }
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
      pending_space = true;
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
  }
  return t;
}
} // namespace cottontail
