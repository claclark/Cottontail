#include "src/recipe.h"

#include <cctype>
#include <map>
#include <regex>
#include <sstream>
#include <string>

#include "src/core.h"

namespace cottontail {

namespace {
inline bool identifier(char c) {
  return c == '-' || c == '_' || c == '.' || isalpha(c) || isdigit(c);
}

const char *munch_string(const char *s, char quote, std::string *return_value) {
  std::string working_value;
  for (; *s != quote && *s != '\0'; s++) {
    if (*s == '\\') {
      s++;
      if (*s != '\\' && *s != quote)
        return s;
    }
    working_value += *s;
  }
  if (*s == '\0')
    return s;
  *return_value = working_value;
  return s;
}

const char *munch_map(const char *s, std::string *return_value) {
  std::string working_value;
  int count = 0;
  for (; (*s != ']' || count > 0) && *s != '\0'; s++) {
    if (*s == ']')
      --count;
    else if (*s == '[')
      count++;
    else if (*s == '\'' || *s == '"') {
      char quote = *s++;
      if (*s == '\0')
        return s;
      working_value += quote;
      for (; *s != quote && *s != '\0'; s++) {
        if (*s == '\\') {
          s++;
          if (*s != '\\' && *s != quote)
            return s;
        }
        working_value += *s;
      }
      if (*s == '\0')
        return s;
    }
    working_value += *s;
  }
  if (*s == '\0')
    return s;
  *return_value = working_value;
  return s;
}

const char *munch(const char *s, char quote, std::string *return_value) {
  if (quote != ']')
    return munch_string(s, quote, return_value);
  else
    return munch_map(s, return_value);
}

const char *parse(const char *s,
                  std::map<std::string, std::string> *return_map) {
  std::map<std::string, std::string> working_map;
  for (; isspace(*s); s++)
    ;
  if (*s != '[')
    return s;
  s++;
  for (;;) {
    for (; isspace(*s); s++)
      ;
    if (*s == ']')
      break;
    if (!identifier(*s))
      return s;
    const char *start = s;
    for (s++; identifier(*s); s++)
      ;
    std::string key(start, s - start);
    for (; isspace(*s); s++)
      ;
    if (*s != ':')
      return s;
    for (s++; isspace(*s); s++)
      ;
    char quote = *s;
    if (quote != '\'' && quote != '"' && quote != '[')
      return s;
    s++;
    std::string value;
    if (quote == '[')
      quote = ']';
    s = munch(s, quote, &value);
    if (*s != quote)
      return s;
    if (quote == ']')
      value = '[' + value + ']';
    working_map[key] = value;
    for (s++; isspace(*s); s++)
      ;
    if (*s == ']')
      break;
    if (*s != ',')
      return s;
    for (s++; isspace(*s); s++)
      ;
    if (*s == ']')
      break;
  }
  for (s++; isspace(*s); s++)
    ;
  if (*s != '\0')
    return s;
  *return_map = working_map;
  return (char *)0;
}

void dump(std::ostream *out,
          const std::map<std::string, std::string> &parameters, size_t depth);

void dump(std::ostream *out, std::string value, size_t depth) {
  std::map<std::string, std::string> parameters;
  if (cook(value, &parameters))
    dump(out, parameters, depth);
  else
    *out << '"' << value << '"';
  *out << ',';
}

void dump(std::ostream *out,
          const std::map<std::string, std::string> &parameters, size_t depth) {
  if (parameters.size() == 0) {
    *out << "[]";
    return;
  }
  *out << "[\n";
  for (auto &parameter : parameters) {
    for (size_t i = 0; i <= depth; i++)
      *out << "  ";
    *out << parameter.first << ':';
    dump(out, parameter.second, depth + 1);
    *out << "\n";
  }
  for (size_t i = 0; i < depth; i++)
    *out << "  ";
  *out << "]";
}

} // namespace

bool cook(const std::string &recipe,
          std::map<std::string, std::string> *parameters, size_t *failure) {
  const char *s = parse(recipe.c_str(), parameters);
  if (s) {
    safe_set(failure) = s - recipe.c_str();
    return false;
  }
  return true;
}

bool cook(const std::string &recipe,
          std::map<std::string, std::string> *parameters, std::string *error) {
  const char *s = parse(recipe.c_str(), parameters);
  if (s) {
    safe_set(error) = "Can't cook recipe";
    return false;
  }
  return true;
}

bool cook(const std::string &recipe,
          std::map<std::string, std::string> *parameters) {
  const char *s = parse(recipe.c_str(), parameters);
  if (s)
    return false;
  else
    return true;
}

std::string freeze(const std::map<std::string, std::string> &parameters) {
  std::stringstream frozen;
  dump(&frozen, parameters, 0);
  frozen << '\n';
  return frozen.str();
}

std::string freeze(std::string value) {
  std::stringstream frozen;
  dump(&frozen, value, 0);
  frozen << '\n';
  return frozen.str();
}

// Intended for command line options
//   X:Z sets the recipe for X to Z
//   X:name:Z sets the name for X to Z, clearing the recipe
//   X:Y:Z sets the value of Y to Z in the recipe for X (where Y != "name")
//   X@Y wraps the recipe for X in wrapper Y
bool interpret_option(std::string *recipe, const std::string &option,
                      std::string *error) {
  if (option == "")
    return true;
  std::map<std::string, std::string> parameters;
  if (!cook(*recipe, &parameters)) {
    safe_set(error) = "Bad recipe";
    return false;
  }
  {
    std::regex sep("@");
    std::vector<std::string> pieces{
        std::sregex_token_iterator(option.begin(), option.end(), sep, -1), {}};
    if (pieces.size() == 2) {
      std::map<std::string, std::string>::iterator item =
          parameters.find(pieces[0]);
      if (item == parameters.end()) {
        safe_set(error) = "Option not found: " + pieces[0];
        return false;
      }
      std::map<std::string, std::string> package;
      package["name"] = pieces[1];
      package["recipe"] = item->second;
      parameters[item->first] = freeze(package);
      *recipe = freeze(parameters);
      return true;
    }
  }
  std::regex sep(":");
  std::vector<std::string> pieces{
      std::sregex_token_iterator(option.begin(), option.end(), sep, -1), {}};
  if (pieces.size() < 2 || pieces.size() > 3) {
    safe_set(error) = "Malformed option: " + option;
  }
  std::map<std::string, std::string>::iterator item =
      parameters.find(pieces[0]);
  if (item == parameters.end()) {
    safe_set(error) = "Option not found: " + pieces[0];
    return false;
  }
  std::map<std::string, std::string> item_parameters;
  if (!cook(item->second, &item_parameters)) {
    safe_set(error) = "There are no options for: " + pieces[0];
    return false;
  }
  if (pieces.size() == 2) {
    item_parameters["recipe"] = pieces[1];
  } else if (pieces[1] == "name") {
    item_parameters["name"] = pieces[2];
    item_parameters["recipe"] = "";
  } else {
    std::map<std::string, std::string>::iterator item_recipe =
        item_parameters.find("recipe");
    if (item_recipe == parameters.end()) {
      safe_set(error) = "Option parameters not found for: " + pieces[0];
      return false;
    }
    std::map<std::string, std::string> item_recipe_parameters;
    if (!cook(item_recipe->second, &item_recipe_parameters)) {
      safe_set(error) = "Malformed option parameters for: " + pieces[0];
      return false;
    }
    std::map<std::string, std::string>::iterator item_recipe_item =
        item_recipe_parameters.find(pieces[1]);
    if (item_recipe_item == item_recipe_parameters.end()) {
      safe_set(error) = "No option: " + pieces[0] + ":" + pieces[1];
      return false;
    }
    item_recipe_parameters[pieces[1]] = pieces[2];
    item_parameters["recipe"] = freeze(item_recipe_parameters);
  }
  parameters[pieces[0]] = freeze(item_parameters);
  *recipe = freeze(parameters);
  return true;
}

// Mostly for testing...
// Extract an option from a recipe. Opposite of interpret_option.
//   X return recipe for X
//   X:name return name for X
//   X:Z return value for Z in the recipe for X
bool extract_option(const std::string &recipe, const std::string &option,
                    std::string *value, std::string *error) {
  std::map<std::string, std::string> parameters;
  if (!cook(recipe, &parameters)) {
    safe_set(error) = "Bad recipe";
    return false;
  }
  std::regex sep(":");
  std::vector<std::string> pieces{
      std::sregex_token_iterator(option.begin(), option.end(), sep, -1), {}};
  if (pieces.size() < 1 || pieces.size() > 3) {
    safe_set(error) = "Malformed option: " + option;
  }
  std::map<std::string, std::string>::iterator item =
      parameters.find(pieces[0]);
  if (item == parameters.end()) {
    safe_set(error) = "Option item not found: " + pieces[0];
    return false;
  }
  std::map<std::string, std::string> item_parameters;
  if (!cook(item->second, &item_parameters)) {
    safe_set(error) = "There are no options for: " + pieces[0];
    return false;
  }
  std::map<std::string, std::string>::iterator item_name =
      item_parameters.find("name");
  if (item_name == parameters.end()) {
    safe_set(error) = "Option setting not found for: " + pieces[0];
    return false;
  }
  std::map<std::string, std::string>::iterator item_recipe =
      item_parameters.find("recipe");
  if (item_recipe == parameters.end()) {
    safe_set(error) = "Option parameters not found for: " + pieces[0];
    return false;
  }
  if (pieces.size() == 1) {
    *value = item_recipe->second;
  } else if (pieces[1] == "name") {
    *value = item_name->second;
  } else {
    std::map<std::string, std::string> item_recipe_parameters;
    if (!cook(item_recipe->second, &item_recipe_parameters)) {
      safe_set(error) = "Malformed option parameters for: " + pieces[0];
      return false;
    }
    std::map<std::string, std::string>::iterator item_recipe_item =
        item_recipe_parameters.find(pieces[1]);
    *value = item_recipe_item->second;
  }
  return true;
}
} // namespace cottontail
