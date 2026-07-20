#ifndef COTTONTAIL_MEADOWLARK_METADATA_H_
#define COTTONTAIL_MEADOWLARK_METADATA_H_

#include <map>
#include <string>

namespace cottontail {
namespace meadowlark {

std::string forager2json(const std::string &name, const std::string &tag,
                         const std::map<std::string, std::string> &parameters);
bool json2forager(const std::string &json, std::string *name, std::string *tag,
                  std::map<std::string, std::string> *parameters,
                  std::string *error = nullptr);

} // namespace meadowlark
} // namespace cottontail

#endif // COTTONTAIL_MEADOWLARK_METADATA_H_
