#ifndef COTTONTAIL_SRC_DNA_H_
#define COTTONTAIL_SRC_DNA_H_

#include "src/core.h"
#include "src/working.h"

namespace cottontail {

const std::string DEFAULT_BURROW = "the.burrow";
const std::string DNA_NAME = "dna"; // parameters

bool read_dna(std::shared_ptr<Working> working, std::string *dna,
              std::string *error = nullptr);
bool write_dna(std::shared_ptr<Working> working, const std::string &dna,
               std::string *error = nullptr);
bool get_parameter_from_dna(std::shared_ptr<Working> working,
                            const std::string &key, std::string *value,
                            std::string *error = nullptr);
bool set_parameter_in_dna(std::shared_ptr<Working> working,
                          const std::string &key, const std::string &value,
                          std::string *error = nullptr);
bool name_and_recipe(const std::string &dna, const std::string &key,
                     std::string *error = nullptr, std::string *name = nullptr,
                     std::string *recipe = nullptr);
} // namespace cottontail

#endif // COTTONTAIL_SRC_DNA_H_
