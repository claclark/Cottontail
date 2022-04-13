#ifndef COTTONTAIL_APPS_COLLECTION_H_
#define COTTONTAIL_APPS_COLLECTION_H_

#include <map>
#include <memory>
#include <string>

#include "src/cottontail.h"

namespace cottontail {

bool duplicates_TREC_CAst(const std::string &location,
                          std::map<std::string, std::string> *canonical,
                          std::string *error);
bool collection_TREC_WaPo2(const std::string &location,
                           std::shared_ptr<cottontail::Builder> builder,
                           std::string *error);
bool collection_TREC_MARCO(const std::string &location,
                           std::shared_ptr<cottontail::Builder> builder,
                           std::string *error);
bool collection_TREC_CAR(const std::string &location,
                         std::shared_ptr<cottontail::Builder> builder,
                         std::string *error);
bool collection_c4(const std::string &location,
                   std::shared_ptr<cottontail::Builder> builder,
                   std::string *error);
bool collection_MSMARCO_V2(const std::string &location,
                           std::shared_ptr<cottontail::Builder> builder,
                           std::string *error);
} // namespace cottontail
#endif // COTTONTAIL_APPS_COLLECTION_H_
