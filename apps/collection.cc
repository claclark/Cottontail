#include "apps/collection.h"

#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <string>

#include "src/cottontail.h"
#include "src/json.h"
#include "src/nlohmann.h"

namespace cottontail {

bool isspace(char c) { return c == ' ' || c == '\t'; }

bool duplicates_TREC_CAst(const std::string &location,
                          std::map<std::string, std::string> *canonical,
                          std::string *error) {
  std::ifstream f(location, std::istream::in);
  if (f.fail()) {
    safe_set(error) = "Cannot open: " + location;
    return false;
  }
  std::string line;
  while (std::getline(f, line)) {
    // parsing more carefully than is probably needed
    const char *s, *e;
    for (s = line.c_str(); isspace(*s); s++)
      ;
    if (*s == ':' || *s == ',' || *s == '\0') {
      safe_set(error) = "Format error in: " + location;
      return false;
    }
    for (e = s + 1; !isspace(*e) && *e != ':' && *e != ',' && *e != '\0'; e++)
      ;
    std::string canon(s, e - s);
    for (; isspace(*e); e++)
      ;
    if (*e != ':') {
      safe_set(error) = "Format error in: " + location;
      return false;
    }
    for (;;) {
      for (s = e + 1; isspace(*s); s++)
        ;
      if (*s == '\0')
        break;
      if (*s == ':' || *s == ',') {
        safe_set(error) = "Format error in: " + location;
        return false;
      }
      for (e = s + 1; !isspace(*e) && *e != ':' && *e != ',' && *e != '\0'; e++)
        ;
      std::string equivalent(s, e - s);
      (*canonical)[equivalent] = canon;
      for (; isspace(*e); e++)
        ;
      if (*e == '\0')
        break;
      if (*e != ',') {
        safe_set(error) = "Format error in: " + location;
        return false;
      }
    }
  }
  return true;
}

bool collection_TREC_WaPo2(const std::string &location,
                           std::shared_ptr<cottontail::Builder> builder,
                           std::string *error) {
  std::ifstream f(location, std::istream::in);
  if (f.fail()) {
    safe_set(error) = "Cannot open: " + location;
    return false;
  }
  std::string line;
  while (std::getline(f, line)) {
    json j;
    try {
      j = json::parse(line);
    } catch (json::parse_error &e) {
      safe_set(error) = "A line in \"" + location + "\" is not json";
      return false;
    }
    try {
      if (j["id"].is_null())
        continue;
      std::string id = j["id"];
      addr p_id, q_id;
      if (!builder->add_text(id, &p_id, &q_id, error))
        return false;
      if (!builder->add_annotation(":id", p_id, q_id, 0.0, error))
        return false;
      addr p_article = p_id, q_article = q_id;
      if (!j["title"].is_null()) {
        addr p_title, q_title;
        if (!builder->add_text(j["title"], &p_title, &q_title, error))
          return false;
        if (!builder->add_annotation(":title", p_title, q_title, 0.0, error))
          return false;
        q_article = q_title;
      }
      if (!j["author"].is_null()) {
        addr p_author, q_author;
        if (!builder->add_text(j["author"], &p_author, &q_author, error))
          return false;
        if (!builder->add_annotation(":author", p_author, q_author, 0.0, error))
          return false;
        q_article = q_author;
      }
      json contents = j["contents"];
      size_t number = 1;
      for (size_t i = 0; i < contents.size(); i++) {
        json element = contents[i];
        std::string content;
        bool is_paragraph = false;
        for (auto &item : element.items()) {
          if (item.key() == "content" || item.key() == "blurb" ||
              item.key() == "fullcaption") {
            if (content != "")
              content += " ";
            if (item.value().is_number()) {
              fval v = item.value();
              content += std::to_string(v);
            } else if (item.value().is_string()) {
              std::string s = item.value();
              content += s;
            }
          } else if (item.key() == "subtype" && item.value().is_string() &&
                     item.value() == "paragraph") {
            is_paragraph = true;
          }
        }
        if (content.size() > 0) {
          addr p_pid, q_pid;
          if (is_paragraph) {
            std::string paragraph_id =
                "WAPO_" + id + "-" + std::to_string(number);
            number++;
            if (!builder->add_text(paragraph_id, &p_pid, &q_pid, error))
              return false;
            if (!builder->add_annotation(":pid", p_pid, q_pid, 0.0, error))
              return false;
          }
          addr p_content, q_content;
          if (!builder->add_text(content, &p_content, &q_content, error))
            return false;
          if (is_paragraph) {
            if (!builder->add_annotation(":paragraph", p_pid, q_content, 0.0,
                                         error))
              return false;
          }
          q_article = q_content;
        }
      }
      if (!j["article_url"].is_null()) {
        addr p_url, q_url;
        if (!builder->add_text(j["article_url"], &p_url, &q_url, error))
          return false;
        if (!builder->add_annotation(":url", p_url, q_url, 0.0, error))
          return false;
        q_article = q_url;
      }
      if (!builder->add_annotation(":article", p_article, q_article, 0.0,
                                   error))
        return false;
    } catch (json::exception &e) {
      safe_set(error) = "TRECWaPo2 format error";
      return false;
    }
  }
  return true;
}

bool collection_TREC_MARCO(const std::string &location,
                           std::shared_ptr<cottontail::Builder> builder,
                           std::string *error) {
  std::ifstream f(location, std::istream::in);
  if (f.fail()) {
    safe_set(error) = "Cannot open: " + location;
    return false;
  }
  std::string line;
  while (std::getline(f, line)) {
    std::string id = "MARCO_" + line.substr(0, line.find("\t"));
    std::string content = line.substr(line.find("\t"));
    addr p_pid, q_pid;
    if (!builder->add_text(id, &p_pid, &q_pid, error))
      return false;
    if (!builder->add_annotation(":pid", p_pid, q_pid, 0.0, error))
      return false;
    addr p_content, q_content;
    if (!builder->add_text(content, &p_content, &q_content, error))
      return false;
    if (!builder->add_annotation(":paragraph", p_pid, q_content, 0.0, error))
      return false;
  }
  return true;
}

namespace {

class CBORBuild {
public:
  CBORBuild(std::shared_ptr<cottontail::Builder> builder) : builder_(builder){};
  CBORBuild(const CBORBuild &) = delete;
  CBORBuild &operator=(const CBORBuild &) = delete;
  CBORBuild(CBORBuild &&) = delete;
  CBORBuild &operator=(CBORBuild &&) = delete;
  void add_text(const std::string &text) {
    if (text.size() > 0) {
      if (content_.size() > 0)
        content_ += " ";
      content_ += text;
    }
  }
  void add_keyword(const std::string &keyword) {
    if (keyword.size() > 0) {
      if (keywords_.size() > 0)
        keywords_ += ", ";
      else
        keywords_ = ">>> ";
      keywords_ += keyword;
    }
  }
  void add_id(const std::string &id) {
    assert(id.size() > 0);
    pid_ = "CAR_" + id;
  }
  bool buildit() {
    if (keywords_.size() > 0) {
      if (content_.size() > 0)
        content_ += " ";
      content_ += keywords_;
    }
    if (pid_.size() > 0 && content_.size() > 0) {
      std::string error;
      addr p_pid, q_pid, p_content, q_content;
      if (!builder_->add_text(pid_, &p_pid, &q_pid, &error))
        return false;
      if (!builder_->add_annotation(":pid", p_pid, q_pid, 0.0, &error))
        return false;
      if (!builder_->add_text(content_, &p_content, &q_content, &error))
        return false;
      if (!builder_->add_annotation(":paragraph", p_pid, q_content, 0.0,
                                    &error))
        return false;
    }
    pid_ = "";
    content_ = "";
    keywords_ = "";
    return true;
  }

private:
  std::shared_ptr<cottontail::Builder> builder_;
  std::string pid_ = "";
  std::string content_ = "";
  std::string keywords_ = "";
};

int getbyte(std::ifstream *f) {
  uint8_t c;
  f->read(reinterpret_cast<char *>(&c), sizeof(c));
  if (f->fail())
    return -1;
  return c;
}

bool CBOR_size(int c, std::ifstream *f, uint64_t *n) {
  *n = 0;
  c = c & 0x1F;
  if (c < 24) {
    *n = c;
    return true;
  } else {
    int d;
    switch (c) {
    case 27:
      if ((d = getbyte(f)) < 0)
        return false;
      *n = d;
      if ((d = getbyte(f)) < 0)
        return false;
      *n = *n * 256 + d;
      if ((d = getbyte(f)) < 0)
        return false;
      *n = *n * 256 + d;
      if ((d = getbyte(f)) < 0)
        return false;
      *n = *n * 256 + d;
      /*FALLTHROUGH*/
    case 26:
      if ((d = getbyte(f)) < 0)
        return false;
      *n = *n * 256 + d;
      if ((d = getbyte(f)) < 0)
        return false;
      *n = *n * 256 + d;
      /*FALLTHROUGH*/
    case 25:
      if ((d = getbyte(f)) < 0)
        return false;
      *n = *n * 256 + d;
      /*FALLTHROUGH*/
    case 24:
      if ((d = getbyte(f)) < 0)
        return false;
      *n = *n * 256 + d;
      return true;
    }
  }
  return false;
}

bool CBOR_text(int c, std::ifstream *f, std::string *text) {
  *text = "";
  uint64_t n;
  if (!CBOR_size(c, f, &n))
    return false;
  for (uint64_t i = 0; i < n; i++) {
    if ((c = getbyte(f)) < 0)
      return false;
    if (c > 32)
      (*text) += (char)(c & 0xFF);
    else
      (*text) += " ";
  }
  return true;
}

bool from_CBOR(std::ifstream *f, CBORBuild *cbor, bool *have_break, int depth,
               int mode) {
  // std::string indent = "";
  // for (int i = 0; i < depth; i++)
  // indent += "  ";
  // indent += "(";
  // indent += std::to_string(mode);
  // indent += ")";

  int c;
  if ((c = getbyte(f)) >= 0) {
    uint64_t n;
    std::string text;
    switch (c & 0xE0) {
    case 0x00: // Positive integer
    case 0x20: // Negative integer
      if (!CBOR_size(c, f, &n))
        return false;
      // std::cout << indent << "###" << n << "\n";
      break;
    case 0x40: // Byte string
    case 0x60: // Text string
      if (!CBOR_text(c, f, &text))
        return false;
      if (mode == 1)
        cbor->add_id(text);
      else if (mode == 2)
        cbor->add_text(text);
      else if (mode == 3)
        cbor->add_keyword(text);
      // std::cout << indent << "---" << text << "\n";
      break;
    case 0x80: // Array
      if (c == 0x9f) {
        // std::cout << indent << "Entering indefinite array\n";
        do {
          if (!from_CBOR(f, cbor, have_break, depth + 1,
                         (mode == 1 ? 2 : mode)))
            return false;
        } while (!(*have_break));
        // std::cout << indent << "Leaving indefinite array\n";
        if (mode == 1)
          cbor->buildit();
      } else {
        if (!CBOR_size(c, f, &n))
          return false;
        // std::cout << indent << "Entering array " << n << "\n";
        for (uint64_t i = 0; i < n; i++) {
          int new_mode = mode;
          if (mode == 0 && depth == 1 && n == 3)
            new_mode = 1;
          if (mode == 2 && depth == 4 && i == 1)
            new_mode = 3;
          if (mode == 2 && depth == 4 && i == 3)
            new_mode = 4;
          if (!from_CBOR(f, cbor, have_break, depth + 1, new_mode))
            return false;
        }
        // std::cout << indent << "Leaving array " << n << "\n";
      }
      break;
    case 0xA0: // Map
      return false;
    case 0xC0: // Tag
      return false;
    case 0xE0: // Primitive
      if (c == 0xFF) {
        // std::cout << indent << "Have break\n";
        *have_break = true;
        return true;
      } else {
        return false;
      }
      break;
    default:
      return false;
    }
    *have_break = false;
  } else {
    *have_break = true;
  }
  return true;
}
} // namespace

bool collection_TREC_CAR(const std::string &location,
                         std::shared_ptr<cottontail::Builder> builder,
                         std::string *error) {
  std::ifstream f(location, std::ios::binary);
  if (f.fail()) {
    safe_set(error) = "Cannot open: " + location;
    return false;
  }
  CBORBuild cbor(builder);
  bool have_break;
  while (from_CBOR(&f, &cbor, &have_break, 0, 0))
    if (have_break)
      return true;
  safe_set(error) = "CBOR parse failure in: " + location;
  return true;
}

bool collection_c4(const std::string &location,
                   std::shared_ptr<cottontail::Builder> builder,
                   std::string *error) {
  std::shared_ptr<std::string> everything = cottontail::inhale(location, error);
  if (everything == nullptr) {
    safe_set(error) = "Cannot open: " + location;
    return false;
  }
  std::string::size_type pos;
  std::string::size_type prev;
  std::string newline = "\n";
  for (prev = 0; (pos = everything->find(newline, prev)) != std::string::npos;
       prev = pos + 1) {
    json j;
    try {
      j = json::parse(everything->substr(prev, pos - prev));
    } catch (json::parse_error &e) {
      safe_set(error) = "Non-json in: " + location;
      return false;
    }
    try {
      if (j["text"].is_null()) {
        safe_set(error) = "Missing text field in: " + location;
        return false;
      }
      if (j["url"].is_null()) {
        safe_set(error) = "Missing url field in: " + location;
        return false;
      }
      std::string text = j["text"];
      addr p_text, q_text;
      if (!builder->add_text(text, &p_text, &q_text, error))
        return false;
      if (!builder->add_annotation(":text", p_text, q_text, 0.0, error))
        return false;
      std::string url = j["url"];
      addr p_url, q_url;
      if (!builder->add_text(url, &p_url, &q_url, error))
        return false;
      if (!builder->add_annotation(":url", p_url, q_url, 0.0, error))
        return false;
      if (!builder->add_annotation(":", p_text, q_url, 0.0, error))
        return false;
    } catch (json::exception &e) {
      safe_set(error) = "C4 format error";
      return false;
    }
  }
  return true;
}

bool collection_MSMARCO_V2(const std::string &location,
                           std::shared_ptr<cottontail::Builder> builder,
                           std::string *error) {
  std::shared_ptr<std::string> everything = cottontail::inhale(location, error);
  if (everything == nullptr) {
    safe_set(error) = "Cannot open: " + location;
    return false;
  }
  std::string::size_type pos;
  std::string::size_type prev;
  std::string newline = "\n";
  for (prev = 0; (pos = everything->find(newline, prev)) != std::string::npos;
       prev = pos + 1) {
    json j;
    try {
      j = json::parse(everything->substr(prev, pos - prev));
    } catch (json::parse_error &e) {
      safe_set(error) = "Non-json in: " + location;
      return false;
    }
    std::string pid = j["pid"];
    std::string paragraph = j["passage"];
    cottontail::addr p_pid, q_pid;
    if (!builder->add_text(pid, &p_pid, &q_pid, error))
      return false;
    if (!builder->add_annotation(":pid", p_pid, q_pid, 0.0, error))
      return false;
    cottontail::addr p_paragraph, q_paragraph;
    if (!builder->add_text(paragraph, &p_paragraph, &q_paragraph, error))
      return false;
    if (!builder->add_annotation(":paragraph", p_pid, q_paragraph, 0.0, error))
      return false;
  }
  return true;
}

bool collection_CAsT2022_preprocessed(
    const std::string &location, std::shared_ptr<cottontail::Builder> builder,
    std::string *error) {
  std::shared_ptr<std::string> everything = cottontail::inhale(location, error);
  if (everything == nullptr) {
    safe_set(error) = "Cannot open: " + location;
    return false;
  }
  std::string::size_type pos;
  std::string::size_type prev;
  std::string newline = "\n";
  for (prev = 0; (pos = everything->find(newline, prev)) != std::string::npos;
       prev = pos + 1) {
    json j;
    try {
      j = json::parse(everything->substr(prev, pos - prev));
    } catch (json::parse_error &e) {
      safe_set(error) = "Non-json in: " + location;
      return false;
    }
    std::string id = j["id"];
    std::string title = j["title"];
    std::string contents = j["contents"];
    cottontail::addr p_id, q_id;
    if (!builder->add_text(id, &p_id, &q_id, error))
      return false;
    if (!builder->add_annotation(":pid", p_id, q_id, 0.0, error))
      return false;
    cottontail::addr p_title, q_title;
    if (!builder->add_text(title, &p_title, &q_title, error))
      return false;
    cottontail::addr p_contents, q_contents;
    if (!builder->add_text(contents, &p_contents, &q_contents, error))
      return false;
    if (!builder->add_annotation(":paragraph", p_id, q_contents, 0.0, error))
      return false;
  }
  return true;
}
} // namespace cottontail
