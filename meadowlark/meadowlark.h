#ifndef COTTONTAIL_MEADOWLARK_MEADOWLARK_H_
#define COTTONTAIL_MEADOWLARK_MEADOWLARK_H_

#include <memory>
#include <string>

#include "src/warren.h"

namespace cottontail {
namespace meadowlark {
std::shared_ptr<Warren> create_meadow(const std::string &meadow,
                                      std::string *error = nullptr);
std::shared_ptr<Warren> create_meadow(std::string *error = nullptr);
std::shared_ptr<Warren> open_meadow(const std::string &meadow,
                                    std::string *error = nullptr);
std::shared_ptr<Warren> open_meadow(std::string *error = nullptr);
bool append_path(std::shared_ptr<Warren> warren, const std::string &filename,
                 addr *path_feature, std::string *error = nullptr);
bool append_tsv(std::shared_ptr<Warren> warren, const std::string &filename,
                std::string *error = nullptr, bool header = false,
                const std::string &separator = "", size_t threads = 0);
bool append_jsonl(std::shared_ptr<Warren> warren, const std::string &filename,
                  std::string *error = nullptr, size_t threads = 0);
} // namespace meadowlark
} // namespace cottontail

#endif // COTTONTAIL_MEADOWLARK_MEADOWLARK_H_
