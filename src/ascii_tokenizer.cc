#include "src/ascii_tokenizer.h"

#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <vector>

#include "src/core.h"
#include "src/featurizer.h"

namespace {

// Working directly with raw ASCII values to avoid possible locale issues.
inline bool alphabetic(char c) {
  return (c >= 0x41 && c <= 0x5A) || (c >= 0x61 && c <= 0x7A);
}

inline bool alphanumeric(char c) {
  return alphabetic(c) || (c >= 0x30 && c <= 0x39);
}

inline char tolower(char c) { return c | 0x20; }

inline bool isupper(char c) { return c >= 0x41 && c <= 0x5A; }

inline char tag_initial(char c) { return alphabetic(c) || c == '_'; }

inline char tag_character(char c) {
  return alphanumeric(c) || c == '-' || c == '_' || c == '.';
}
} // namespace

// tokenize_, skip_ and split_ are semi-cloned versions of one another,
// which is not ideal.

namespace cottontail {

std::vector<Token>
AsciiTokenizer::tokenize_(std::shared_ptr<Featurizer> featurizer, char *buffer,
                          size_t length) {
  std::vector<Token> tokens;
  char *p = buffer;
  char *last = buffer + length;
  addr address = 0;
  for (;;) {
    for (; p != last && !alphanumeric(*p); p++)
      if (xml_) {
        if (*p == '<') {
          // Tokenize an XML/SGML/HTML tag
          char *q = p + 1;
          if (q != last && *q == '/')
            q++;
          if (q != last && tag_initial(*q)) {
            for (q++; q != last && tag_character(*q); q++)
              ;
            if (q != last && *q != '\n') {
              char *r;
              for (r = q; r != last && *r != '\n' && *r != '>'; r++)
                ;
              if (r != last && *r == '>') {
                *q = '>';
                addr feature = featurizer->featurize(p, ((q + 1) - p));
                size_t offset = p - buffer;
                size_t length = (r + 1) - p;
                tokens.emplace_back(feature, address, offset, length);
                address++;
                p = r;
              }
            }
          }
        } else if (*p == '&') {
          // Skip an XML/SGML/HTML entity
          char *q;
          for (q = p + 1; q != last && *q != ' ' && *q != '\n' && *q != ';';
               q++)
            ;
          if (q != last && *q == ';')
            p = q;
        }
      }
    if (p == last)
      return tokens;
    size_t offset = p - buffer;
    size_t uppers = 0;
    char *q;
    for (q = p + 1; q != last && alphanumeric(*q); q++)
      if (isupper(*q))
        uppers++;
    size_t length = q - p;
    if (uppers > 0) {
      addr feature = featurizer->featurize(p, q - p);
      tokens.emplace_back(feature, address, offset, length);
      for (char *r = p; r < q; r++)
        *r = tolower(*r);
    } else {
      *p = tolower(*p);
    }
    addr feature = featurizer->featurize(p, q - p);
    tokens.emplace_back(feature, address, offset, length);
    address++;
    p = q;
  }
}

const char *AsciiTokenizer::skip_(const char *buffer, size_t length, addr n) {
  const char *p = buffer;
  const char *last = buffer + length;
  for (;;) {
    for (; p != last && !alphanumeric(*p); p++)
      if (xml_) {
        if (*p == '<') {
          // Skip an XML/SGML/HTML tag
          const char *q = p + 1;
          if (q != last && *q == '/')
            q++;
          if (q != last && tag_initial(*q)) {
            for (q++; q != last && tag_character(*q); q++)
              ;
            if (q != last && *q != '\n') {
              for (; q != last && *q != '\n' && *q != '>'; q++)
                ;
              if (q != last && *q == '>') {
                if (n <= 0)
                  return p;
                else
                  --n;
                p = q;
              }
            }
          }
        } else if (*p == '&') {
          // Skip an XML/SGML/HTML entity
          const char *q;
          for (q = p + 1; q != last && *q != ' ' && *q != '\n' && *q != ';';
               q++)
            ;
          if (q != last && *q == ';')
            p = q;
        }
      }
    if (p == last)
      return last;
    if (n <= 0)
      return p;
    for (; p != last && alphanumeric(*p); p++)
      ;
    --n;
  }
}

std::vector<std::string> AsciiTokenizer::split_(const std::string &text) {
  std::vector<std::string> terms;
  std::unique_ptr<char[]> buffer(new char[text.length() + 1]);
  memcpy(buffer.get(), text.c_str(), text.size());
  buffer.get()[text.size()] = '\0';
  char *p = buffer.get();
  char *last = p + text.length();
  for (;;) {
    for (; p != last && !alphanumeric(*p); p++)
      if (xml_) {
        if (*p == '<') {
          // Tokenize an XML/SGML/HTML tag
          char *q = p + 1;
          if (q != last && *q == '/')
            q++;
          if (q != last && tag_initial(*q)) {
            for (q++; q != last && tag_character(*q); q++)
              ;
            if (q != last && *q != '\n') {
              char *r;
              for (r = q; r != last && *r != '\n' && *r != '>'; r++)
                ;
              if (r != last && *r == '>') {
                *q = '>';
                terms.emplace_back(p, (q + 1) - p);
                p = r;
              }
            }
          }
        } else if (*p == '&') {
          // Skip an XML/SGML/HTML entity
          char *q;
          for (q = p + 1; q != last && *q != ' ' && *q != '\n' && *q != ';';
               q++)
            ;
          if (q != last && *q == ';')
            p = q;
        }
      }
    if (p == last)
      return terms;
    size_t uppers = 0;
    char *q;
    for (q = p + 1; q != last && alphanumeric(*q); q++)
      if (isupper(*q))
        uppers++;
    if (uppers == 0)
      *p = tolower(*p);
    terms.emplace_back(p, q - p);
    p = q;
  }
}

std::string AsciiTokenizer::recipe_() {
  if (xml_)
    return "xml";
  else
    return "noxml";
}

std::shared_ptr<Tokenizer> AsciiTokenizer::make(const std::string &recipe,
                                                std::string *error) {
  if (recipe == "" || recipe == "noxml") {
    return std::make_shared<AsciiTokenizer>(false);
  } else if (recipe == "xml") {
    return std::make_shared<AsciiTokenizer>(true);
  } else {
    safe_set(error) = "Can't make AsciiTokenizer from recipe: " + recipe;
    return nullptr;
  }
}

bool AsciiTokenizer::check(const std::string &recipe, std::string *error) {
  if (recipe == "" || recipe == "xml" || recipe == "noxml") {
    return true;
  } else {
    safe_set(error) = "Bad AsciiTokenizer recipe: " + recipe;
    return false;
  }
}
} // namespace cottontail
