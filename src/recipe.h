#ifndef COTTONTAIL_SRC_RECIPE_H_
#define COTTONTAIL_SRC_RECIPE_H_

#include <map>
#include <string>

namespace cottontail {
bool cook(const std::string &recipe,
          std::map<std::string, std::string> *parameters, size_t *failure);
bool cook(const std::string &recipe,
          std::map<std::string, std::string> *parameters, std::string *error);
bool cook(const std::string &recipe,
          std::map<std::string, std::string> *parameters);
std::string freeze(const std::map<std::string, std::string> &parameters);
std::string freeze(std::string value);
bool interpret_option(std::string *recipe, const std::string &option,
                      std::string *error = nullptr);
bool extract_option(const std::string &recipe, const std::string &option,
                    std::string *value, std::string *error = nullptr);
} // namespace cottontail
#endif // COTTONTAIL_SRC_RECIPE_H_
