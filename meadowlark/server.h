#ifndef COTTONTAIL_MEADOWLARK_SERVER_H_
#define COTTONTAIL_MEADOWLARK_SERVER_H_

#include <string>
#include <vector>

namespace cottontail {
namespace meadowlark {

constexpr char SERVER_SOCKET_NAME[] = "meadowlark.sock";

bool check_server_owner(const std::string &dna, std::string *error = nullptr);
std::string server_path(const std::string &executable,
                         std::string *error = nullptr);
bool config(const std::string &meadow, const std::string &executable,
            std::string *error = nullptr);
bool config(const std::string &meadow, const std::string &executable,
            const std::vector<std::string> &arguments,
            std::string *error = nullptr);
bool server(const std::string &burrow, std::string *error = nullptr);

// True when the burrow lock can be acquired and released.
bool shutdown(const std::string &burrow, std::string *error = nullptr);

} // namespace meadowlark
} // namespace cottontail

#endif // COTTONTAIL_MEADOWLARK_SERVER_H_
