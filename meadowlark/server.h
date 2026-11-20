#ifndef COTTONTAIL_MEADOWLARK_SERVER_H_
#define COTTONTAIL_MEADOWLARK_SERVER_H_

#include <string>

namespace cottontail {
namespace meadowlark {

bool server(const std::string &burrow, std::string *error = nullptr);

} // namespace meadowlark
} // namespace cottontail

#endif // COTTONTAIL_MEADOWLARK_SERVER_H_
