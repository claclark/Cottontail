#ifndef COTTONTAIL_APPS_WALK_H_
#define COTTONTAIL_APPS_WALK_H_

#include <exception>
#include <regex>
#include <string>
#include <vector>

#include <boost/filesystem.hpp>

namespace cottontail {
bool walk_filesystem(char *name, std::vector<std::string> *text);
}
#endif // COTTONTAIL_APPS_WALK_H_
