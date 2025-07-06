#ifndef COTTONTAIL_SRC_MEADOWLARK_MEADOWLARK_H_
#define COTTONTAIL_SRC_MEADOWLARK_MEADOWLARK_H_

#include <memory>
#include <string>

#include "src/warren.h"

namespace cottontail {
namespace meadowlark {
bool append_path(std::shared_ptr<Warren> warren, const std::string &filename,
                 addr *path_feature, std::string *error = nullptr);
}
} // namespace cottontail

#endif // COTTONTAIL_SRC_MEADOWLARK_MEADOWLARK_H_
