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

} // namespace cottontail

#endif // COTTONTAIL_SRC_DNA_H_
