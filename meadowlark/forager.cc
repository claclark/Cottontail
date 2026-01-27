#include "meadowlark/forager.h"

#include <cctype>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "meadowlark/null_forager.h"
#include "meadowlark/tf-idf_forager.h"
#include "src/annotator.h"
#include "src/core.h"
#include "src/tokenizer.h"
#include "src/warren.h"

namespace cottontail {
namespace meadowlark {

std::string forager_label(const std::string &name, const std::string &tag) {
  std::string combined_tag;
  if (name != "")
    combined_tag = name + ":";
  if (tag != "")
    combined_tag += tag + ":";
  if (combined_tag == "")
    combined_tag = "tf-idf:";
  return combined_tag;
}

std::shared_ptr<Forager>
Forager::make(std::shared_ptr<Warren> warren, const std::string &name,
              const std::string &tag,
              const std::map<std::string, std::string> &parameters,
              std::string *error) {
  std::shared_ptr<Forager> forager = nullptr;
  std::string combined_tag = forager_label(name, tag);
  if (name == "null")
    forager = NullForager::make(warren, combined_tag, parameters, error);
  else if (name == "" || name == "tf-idf")
    forager = TfIdfForager::make(warren, combined_tag, parameters, error);
  else
    safe_error(error) = "No Forager named: " + name;
  if (forager != nullptr) {
    forager->name_ = name;
    forager->tag_ = tag;
    forager->parameters_ = parameters;
    forager->warren_ = warren;
  }
  return forager;
}

bool Forager::check(const std::string &name, const std::string &tag,
                    const std::map<std::string, std::string> &parameters,
                    std::string *error) {
  std::string combined_tag = forager_label(name, tag);
  if (name == "null")
    return NullForager::check(combined_tag, parameters, error);
  else if (name == "" || name == "tf-idf")
    return TfIdfForager::check(combined_tag, parameters, error);
  safe_error(error) = "No Forager named: " + name;
  return false;
}

bool Forager::label(std::string *error) {
  addr p, q;
  std::string label = "@" + forager_label(name_, tag_);
  if (!warren_->appender()->append(forager2json(name_, tag_, parameters_), &p,
                                   &q, error) ||
      !warren_->annotator()->annotate(warren_->featurizer()->featurize(label),
                                      p, q, error) ||
      !warren_->annotator()->annotate(warren_->featurizer()->featurize("@"), p,
                                      q, error))
    return false;
  else
    return true;
}

namespace {

// ---------- JSON writer helpers ----------

static void append_escaped_json_string(std::string *out,
                                       const std::string &in) {
  out->push_back('"');
  for (unsigned char uc : in) {
    char c = static_cast<char>(uc);
    switch (c) {
    case '\"':
      out->append("\\\"");
      break;
    case '\\':
      out->append("\\\\");
      break;
    case '\b':
      out->append("\\b");
      break;
    case '\f':
      out->append("\\f");
      break;
    case '\n':
      out->append("\\n");
      break;
    case '\r':
      out->append("\\r");
      break;
    case '\t':
      out->append("\\t");
      break;
    default:
      if (uc < 0x20) {
        static const char hex[] = "0123456789abcdef";
        out->append("\\u00");
        out->push_back(hex[(uc >> 4) & 0xF]);
        out->push_back(hex[uc & 0xF]);
      } else {
        out->push_back(c);
      }
      break;
    }
  }
  out->push_back('"');
}

// ---------- JSON reader helpers (subset) ----------

static void skip_nulls(const std::string &s, size_t *i) {
  while (*i < s.size() && static_cast<unsigned char>(s[*i]) == '\0')
    ++(*i);
}

static void skip_ws(const std::string &s, size_t *i) {
  while (*i < s.size() && std::isspace(static_cast<unsigned char>(s[*i])))
    ++(*i);
}

static bool consume(const std::string &s, size_t *i, char c) {
  skip_ws(s, i);
  if (*i < s.size() && s[*i] == c) {
    ++(*i);
    return true;
  }
  return false;
}

static bool expect(const std::string &s, size_t *i, char c) {
  return consume(s, i, c);
}

static int hexval(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return 10 + (c - 'a');
  if (c >= 'A' && c <= 'F')
    return 10 + (c - 'A');
  return -1;
}

static bool parse_string(const std::string &s, size_t *i, std::string *out) {
  skip_ws(s, i);
  if (*i >= s.size() || s[*i] != '"')
    return false;
  ++(*i);
  std::string r;
  while (*i < s.size()) {
    char c = s[(*i)++];
    if (c == '"') {
      *out = r;
      return true;
    }
    if (c == '\\') {
      if (*i >= s.size())
        return false;
      char e = s[(*i)++];
      switch (e) {
      case '"':
        r.push_back('"');
        break;
      case '\\':
        r.push_back('\\');
        break;
      case '/':
        r.push_back('/');
        break;
      case 'b':
        r.push_back('\b');
        break;
      case 'f':
        r.push_back('\f');
        break;
      case 'n':
        r.push_back('\n');
        break;
      case 'r':
        r.push_back('\r');
        break;
      case 't':
        r.push_back('\t');
        break;
      case 'u': {
        // minimal \uXXXX support; roundtrips your own output (\u00XX).
        if (*i + 4 > s.size())
          return false;
        int h0 = hexval(s[(*i)++]);
        int h1 = hexval(s[(*i)++]);
        int h2 = hexval(s[(*i)++]);
        int h3 = hexval(s[(*i)++]);
        if (h0 < 0 || h1 < 0 || h2 < 0 || h3 < 0)
          return false;
        int code = (h0 << 12) | (h1 << 8) | (h2 << 4) | h3;
        if (code >= 0 && code <= 255)
          r.push_back(static_cast<char>(code));
        else
          r.push_back('?');
        break;
      }
      default:
        return false;
      }
    } else {
      if (static_cast<unsigned char>(c) < 0x20)
        return false;
      r.push_back(c);
    }
  }
  return false;
}

static bool skip_number(const std::string &s, size_t *i) {
  skip_ws(s, i);
  size_t start = *i;
  if (*i < s.size() && (s[*i] == '-' || s[*i] == '+'))
    ++(*i);

  bool any = false;
  while (*i < s.size() && std::isdigit(static_cast<unsigned char>(s[*i]))) {
    ++(*i);
    any = true;
  }
  if (!any) {
    *i = start;
    return false;
  }

  if (*i < s.size() && s[*i] == '.') {
    ++(*i);
    bool frac = false;
    while (*i < s.size() && std::isdigit(static_cast<unsigned char>(s[*i]))) {
      ++(*i);
      frac = true;
    }
    if (!frac) {
      *i = start;
      return false;
    }
  }

  if (*i < s.size() && (s[*i] == 'e' || s[*i] == 'E')) {
    ++(*i);
    if (*i < s.size() && (s[*i] == '+' || s[*i] == '-'))
      ++(*i);
    bool exp = false;
    while (*i < s.size() && std::isdigit(static_cast<unsigned char>(s[*i]))) {
      ++(*i);
      exp = true;
    }
    if (!exp) {
      *i = start;
      return false;
    }
  }

  return true;
}

static bool skip_literal(const std::string &s, size_t *i, const char *lit) {
  skip_ws(s, i);
  size_t start = *i;
  for (size_t k = 0; lit[k]; ++k) {
    if (*i >= s.size() || s[*i] != lit[k]) {
      *i = start;
      return false;
    }
    ++(*i);
  }
  return true;
}

static bool skip_value(const std::string &s, size_t *i);

static bool skip_array(const std::string &s, size_t *i) {
  if (!expect(s, i, '['))
    return false;
  skip_ws(s, i);
  if (consume(s, i, ']'))
    return true;
  while (true) {
    if (!skip_value(s, i))
      return false;
    skip_ws(s, i);
    if (consume(s, i, ']'))
      return true;
    if (!expect(s, i, ','))
      return false;
  }
}

static bool skip_object(const std::string &s, size_t *i) {
  if (!expect(s, i, '{'))
    return false;
  skip_ws(s, i);
  if (consume(s, i, '}'))
    return true;
  while (true) {
    std::string k;
    if (!parse_string(s, i, &k))
      return false;
    if (!expect(s, i, ':'))
      return false;
    if (!skip_value(s, i))
      return false;
    skip_ws(s, i);
    if (consume(s, i, '}'))
      return true;
    if (!expect(s, i, ','))
      return false;
  }
}

static bool skip_value(const std::string &s, size_t *i) {
  skip_ws(s, i);
  if (*i >= s.size())
    return false;
  char c = s[*i];
  if (c == '"') {
    std::string tmp;
    return parse_string(s, i, &tmp);
  }
  if (c == '{')
    return skip_object(s, i);
  if (c == '[')
    return skip_array(s, i);
  if (c == 't')
    return skip_literal(s, i, "true");
  if (c == 'f')
    return skip_literal(s, i, "false");
  if (c == 'n')
    return skip_literal(s, i, "null");
  return skip_number(s, i);
}

static bool
parse_parameters_object(const std::string &s, size_t *i,
                        std::map<std::string, std::string> *parameters) {
  if (!expect(s, i, '{'))
    return false;
  skip_ws(s, i);
  if (consume(s, i, '}'))
    return true;

  while (true) {
    std::string k, v;
    if (!parse_string(s, i, &k))
      return false;
    if (!expect(s, i, ':'))
      return false;
    if (!parse_string(s, i, &v))
      return false;
    (*parameters)[k] = v;

    skip_ws(s, i);
    if (consume(s, i, '}'))
      return true;
    if (!expect(s, i, ','))
      return false;
  }
}

} // namespace

// ---------- public API ----------

std::string forager2json(const std::string &name, const std::string &tag,
                         const std::map<std::string, std::string> &parameters) {
  std::string out;
  out += "{\n  \"name\": ";
  append_escaped_json_string(&out, name);
  out += ",\n  \"tag\": ";
  append_escaped_json_string(&out, tag);
  out += ",\n  \"parameters\": {\n";

  bool first = true;
  for (const auto &kv : parameters) {
    if (!first)
      out += ",\n";
    first = false;
    out += "    ";
    append_escaped_json_string(&out, kv.first);
    out += ": ";
    append_escaped_json_string(&out, kv.second);
  }

  out += "\n  }\n}\n";
  return out;
}

namespace {

bool json2forager0(const std::string &json, std::string *name, std::string *tag,
                   std::map<std::string, std::string> *parameters) {
  if (!name || !tag || !parameters)
    return false;

  *name = "";
  *tag = "";
  parameters->clear();

  size_t i = 0;
  if (!expect(json, &i, '{'))
    return false;

  bool saw_name = false;
  bool saw_tag = false;

  skip_ws(json, &i);
  if (!consume(json, &i, '}')) {
    while (true) {
      std::string key;
      if (!parse_string(json, &i, &key))
        return false;
      if (!expect(json, &i, ':'))
        return false;

      if (key == "name") {
        if (!parse_string(json, &i, name))
          return false;
        saw_name = true;
      } else if (key == "tag") {
        if (!parse_string(json, &i, tag))
          return false;
        saw_tag = true;
      } else if (key == "parameters") {
        skip_ws(json, &i);
        if (i < json.size() && json[i] == '{') {
          if (!parse_parameters_object(json, &i, parameters))
            return false;
        } else {
          if (!skip_value(json, &i))
            return false;
        }
      } else {
        if (!skip_value(json, &i))
          return false;
      }

      skip_ws(json, &i);
      if (consume(json, &i, '}'))
        break;
      if (!expect(json, &i, ','))
        return false;
    }
  }

  skip_ws(json, &i);
  skip_nulls(json, &i);
  if (i != json.size())
    return false;
  if (!saw_name)
    return false;
  if (!saw_tag)
    *tag = "";
  return true;
}
} // namespace

bool json2forager(const std::string &json, std::string *name, std::string *tag,
                  std::map<std::string, std::string> *parameters,
                  std::string *error) {
  if (!json2forager0(json, name, tag, parameters)) {
    safe_set(error) = "Error parsing forager from json";
    return false;
  } else {
    return true;
  }
}

} // namespace meadowlark
} // namespace cottontail
