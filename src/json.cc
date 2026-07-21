#include "src/json.h"

#include <cassert>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>

#include "src/core.h"
#include "src/nlohmann.h"
#include "src/scribe.h"

namespace cottontail {

namespace {
template <typename Target>
bool annotate(const std::shared_ptr<Target> &target,
              const std::string &feature, addr p, addr q, fval v,
              std::string *error) {
  if (feature == "")
    return true;
  return target->annotator()->annotate(target->featurizer()->featurize(feature),
                                       p, q, v, error);
}

template <typename Target>
bool do_json(json &j, const std::shared_ptr<Target> &target,
             const std::string &path, const std::string &feature, addr *p,
             addr *q, std::string *error);

template <typename Target>
bool do_null(json &j, const std::shared_ptr<Target> &target,
             const std::string &path, const std::string &feature, addr *p,
             addr *q, std::string *error) {
  if (!target->appender()->append("null", p, q, error))
    return false;
  return annotate(target, feature, *p, *q, NAN, error);
}

template <typename Target>
bool do_boolean(json &j, const std::shared_ptr<Target> &target,
                const std::string &path, const std::string &feature, addr *p,
                addr *q, std::string *error) {
  if (j) {
    if (!target->appender()->append("true", p, q, error))
      return false;
    return annotate(target, feature, *p, *q, 1.0, error);
  } else {
    if (!target->appender()->append("false", p, q, error))
      return false;
    return annotate(target, feature, *p, *q, 0.0, error);
  }
  return true;
}

template <typename Target>
bool do_number(json &j, const std::shared_ptr<Target> &target,
               const std::string &path, const std::string &feature, addr *p,
               addr *q, std::string *error) {
  fval v = j;
  std::string s = open_number_token + std::to_string(v) + close_number_token;
  if (!target->appender()->append(s, p, q, error))
    return false;
  return annotate(target, feature, *p, *q, v, error);
}

template <typename Target>
bool do_string(json &j, const std::shared_ptr<Target> &target,
               const std::string &path, const std::string &feature, addr *p,
               addr *q, std::string *error) {
  std::string s = open_string_token + (std::string)j + close_string_token;
  if (!target->appender()->append(s, p, q, error))
    return false;
  return annotate(target, feature, *p, *q, 0.0, error);
}

template <typename Target>
bool do_array(json &j, const std::shared_ptr<Target> &target,
              const std::string &path, const std::string &feature, addr *p,
              addr *q, std::string *error) {
  addr p0, q0;
  if (!target->appender()->append(open_array_token, p, &q0, error))
    return false;
  size_t index = 0;
  for (json::iterator it = j.begin(); it != j.end(); it++) {
    if (it != j.begin()) {
      if (!target->appender()->append(comma_token, &p0, &q0, error))
        return false;
    }
    std::string element = "[" + std::to_string(index) + "]:";
    std::string child = path + element;
    if (!do_json(it.value(), target, child, child, &p0, &q0, error))
      return false;
    index++;
  }
  if (!target->appender()->append(close_array_token, &p0, q, error))
    return false;
  return annotate(target, feature, *p, *q, (fval)j.size(), error);
}

template <typename Target>
bool do_object(json &j, const std::shared_ptr<Target> &target,
               const std::string &path, const std::string &feature, addr *p,
               addr *q, std::string *error) {
  addr p0, q0;
  if (!target->appender()->append(open_object_token, p, &q0, error))
    return false;
  for (json::iterator it = j.begin(); it != j.end(); it++) {
    if (it != j.begin()) {
      if (!target->appender()->append(comma_token, &p0, &q0, error))
        return false;
    }
    std::string key =
        open_string_token + it.key() + close_string_token + colon_token;
    if (!target->appender()->append(key, &p0, &q0, error))
      return false;
    std::string child = path + it.key() + ":";
    if (!do_json(it.value(), target, child, child, &p0, &q0, error))
      return false;
    if (!annotate(target, it.key() + ":", p0, q0, 0.0, error))
      return false;
  }
  if (!target->appender()->append(close_object_token, &p0, q, error))
    return false;
  return annotate(target, feature, *p, *q, 0.0, error);
}

template <typename Target>
bool do_json(json &j, const std::shared_ptr<Target> &target,
             const std::string &path, const std::string &feature, addr *p,
             addr *q, std::string *error) {
  if (j.is_null())
    return do_null(j, target, path, feature, p, q, error);
  else if (j.is_boolean())
    return do_boolean(j, target, path, feature, p, q, error);
  else if (j.is_number())
    return do_number(j, target, path, feature, p, q, error);
  else if (j.is_string())
    return do_string(j, target, path, feature, p, q, error);
  else if (j.is_array())
    return do_array(j, target, path, feature, p, q, error);
  else if (j.is_object())
    return do_object(j, target, path, feature, p, q, error);
  safe_error(error) = "Unknown JSON data type.";
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

bool parse_json(const std::string &s, json *j, std::string *error) {
  try {
    if (contains_utf8_noncharacters(s))
      *j = json::parse(sanitize(s));
    else
      *j = json::parse(s);
  } catch (const json::parse_error &e) {
    safe_error(error) = "Cannot parse json: " + std::string(e.what());
    return false;
  }
  return true;
}
} // namespace

bool json_scribe(const std::string &s, std::shared_ptr<Scribe> scribe, addr *p,
                 addr *q, std::string *error) {
  assert(scribe != nullptr);
  json j;
  if (!parse_json(s, &j, error))
    return false;
  return do_json(j, scribe, ":", ":", p, q, error);
}

bool json_scribe(const std::string &s, std::shared_ptr<Scribe> scribe,
                 std::string *error) {
  addr p, q;
  return json_scribe(s, scribe, &p, &q, error);
}

bool json_append(const std::string &s, std::shared_ptr<Warren> warren, addr *p,
                 addr *q, const std::string &feature, std::string *error) {
  assert(warren != nullptr);
  json j;
  if (!parse_json(s, &j, error))
    return false;
  return do_json(j, warren, ":", feature, p, q, error);
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
    } else if (inside) {
      if (*c == '"')
        t += "\\\"";
      else if (*c == '\\')
        t += "\\\\";
      else if (*c == '\b')
        t += "\\b";
      else if (*c == '\f')
        t += "\\f";
      else if (*c == '\n')
        t += "\\n";
      else if (*c == '\r')
        t += "\\r";
      else if (*c == '\t')
        t += "\\t";
      else if (static_cast<unsigned char>(*c) < 0x20) {
        static const char hex[] = "0123456789abcdef";
        unsigned char u = static_cast<unsigned char>(*c);
        t += "\\u00";
        t += hex[(u >> 4) & 0x0f];
        t += hex[u & 0x0f];
      } else
        t += *c;
    } else if (*c == '\n') {
      pending_space = true;
    } else {
      t += *c;
    }
  }
  return t;
}

std::string json_encode(const std::string &input) {
  std::ostringstream o;
  o << cottontail::open_string_token;
  for (unsigned char c : input) {
    switch (c) {
    case '"':
      o << "\\\"";
      break;
    case '\\':
      o << "\\\\";
      break;
    case '\b':
      o << "\\b";
      break;
    case '\f':
      o << "\\f";
      break;
    case '\n':
      o << "\\n";
      break;
    case '\r':
      o << "\\r";
      break;
    case '\t':
      o << "\\t";
      break;
    default:
      if (c < 0x20) {
        o << "\\u" << std::hex << std::setw(4) << std::setfill('0')
          << static_cast<int>(c) << std::dec << std::setw(0);
      } else {
        o << c;
      }
    }
  }
  o << cottontail::close_string_token;
  return o.str();
}
} // namespace cottontail
