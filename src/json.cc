#include "src/json.h"

#include <memory>
#include <string>

#include "src/core.h"
#include "src/json.h"
#include "src/scribe.h"

namespace cottontail {

namespace {
bool do_json(json &j, std::shared_ptr<Scribe> scribe, const std::string &tag,
             addr *p, addr *q, std::string *error);

bool do_null(json &j, std::shared_ptr<Scribe> scribe, const std::string &tag,
             addr *p, addr *q, std::string *error) {
  if (!scribe->appender()->append("null", p, q, error))
    return false;
  return scribe->annotator()->annotate(scribe->featurizer()->featurize(tag), *p,
                                       *q, NAN, error);
}

bool do_boolean(json &j, std::shared_ptr<Scribe> scribe, const std::string &tag,
                addr *p, addr *q, std::string *error) {
  if (j) {
    if (!scribe->appender()->append("true", p, q, error))
      return false;
    if (!scribe->annotator()->annotate(scribe->featurizer()->featurize(tag), *p,
                                       *q, 1.0, error))
      return false;
  } else {
    if (!scribe->appender()->append("false", p, q, error))
      return false;
    if (!scribe->annotator()->annotate(scribe->featurizer()->featurize(tag), *p,
                                       *q, 0.0, error))
      return false;
  }
  return true;
}

bool do_number(json &j, std::shared_ptr<Scribe> scribe, const std::string &tag,
               addr *p, addr *q, std::string *error) {
  fval v = j;
  if (!scribe->appender()->append(std::to_string(v), p, q, error))
    return false;
  return scribe->annotator()->annotate(scribe->featurizer()->featurize(tag), *p,
                                       *q, v, error);
}

bool do_string(json &j, std::shared_ptr<Scribe> scribe, const std::string &tag,
               addr *p, addr *q, std::string *error) {
  std::string s = j;
  std::string t = "\"";
  for (const char *p = s.c_str(); *p; p++)
    if (*p == '"')
      t += "\\\"";
    else
      t += *p;
  t += "\"";
  if (!scribe->appender()->append(t, p, q, error))
    return false;
  return scribe->annotator()->annotate(scribe->featurizer()->featurize(tag), *p,
                                       *q, 0.0, error);
}

bool do_array(json &j, std::shared_ptr<Scribe> scribe, const std::string &tag,
              addr *p, addr *q, std::string *error) {
  addr p0, q0;
  addr p_min = maxfinity, q_max = minfinity;
  if (!scribe->appender()->append("[", &p0, &q0, error))
    return false;
  p_min = std::min(p_min, p0);
  q_max = std::max(q_max, q0);
  size_t index = 0;
  for (json::iterator it = j.begin(); it != j.end(); it++) {
    if (!do_json(it.value(), scribe, tag + std::to_string(index) + ":", &p0,
                 &q0, error))
      return false;
    p_min = std::min(p_min, p0);
    q_max = std::max(q_max, q0);
    index++;
  }
  if (!scribe->appender()->append("]", &p0, &q0, error))
    return false;
  // p_min = std::min(p_min, p0);
  // q_max = std::max(q_max, q0);
  if (!scribe->annotator()->annotate(scribe->featurizer()->featurize(tag),
                                     p_min, q_max, (fval)j.size(), error))
    return false;
  *p = p_min;
  *q = q_max;
  return true;
}

bool do_object(json &j, std::shared_ptr<Scribe> scribe, const std::string &tag,
               addr *p, addr *q, std::string *error) {
  addr p0, q0;
  addr p_min = maxfinity, q_max = minfinity;
  if (!scribe->appender()->append("{", &p0, &q0, error))
    return false;
  p_min = std::min(p_min, p0);
  q_max = std::max(q_max, q0);
  for (json::iterator it = j.begin(); it != j.end(); it++) {
    std::string key;
    if (it != j.begin())
      key = ",\"" + it.key() + "\":";
    else
      key = "\"" + it.key() + "\":";
    if (!scribe->appender()->append(key, &p0, &q0, error))
      return false;
    p_min = std::min(p_min, p0);
    q_max = std::max(q_max, q0);
    if (!do_json(it.value(), scribe, tag + it.key() + ":", &p0, &q0, error))
      return false;
    p_min = std::min(p_min, p0);
    q_max = std::max(q_max, q0);
  }
  if (!scribe->appender()->append("}", &p0, &q0, error))
    return false;
  // p_min = std::min(p_min, p0);
  // q_max = std::max(q_max, q0);
  if (!scribe->annotator()->annotate(scribe->featurizer()->featurize(tag),
                                     p_min, q_max, 0.0, error))
    return false;
  *p = p_min;
  *q = q_max;
  return true;
}

bool do_json(json &j, std::shared_ptr<Scribe> scribe, const std::string &tag,
             addr *p, addr *q, std::string *error) {
  if (j.is_null())
    return do_null(j, scribe, tag, p, q, error);
  else if (j.is_boolean())
    return do_boolean(j, scribe, tag, p, q, error);
  else if (j.is_number())
    return do_number(j, scribe, tag, p, q, error);
  else if (j.is_string())
    return do_string(j, scribe, tag, p, q, error);
  else if (j.is_array())
    return do_array(j, scribe, tag, p, q, error);
  else if (j.is_object())
    return do_object(j, scribe, tag, p, q, error);
  safe_set(error) = "Unknown JSON data type.";
  return false;
}
} // namespace

bool scribe_json(json &j, std::shared_ptr<Scribe> scribe, std::string *error) {
  if (scribe == nullptr) {
    safe_set(error) = "Function scribe_json passed null scribe";
    return false;
  }
  if (!j.is_object()) {
    safe_set(error) = "Function scribe_json can only scribe JSON objects";
    return false;
  }
  addr p, q;
  return do_json(j, scribe, ":", &p, &q, error);
  return true;
}

} // namespace cottontail
