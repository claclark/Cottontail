#ifndef COTTONTAIL_MEADOWLARK_METADATA_H_
#define COTTONTAIL_MEADOWLARK_METADATA_H_

#include <map>
#include <string>
#include <vector>

namespace cottontail {
namespace meadowlark {

struct ForagerMetadata {
  std::string name;
  std::string tag;
  std::string query;
  std::string filename;
  std::map<std::string, std::string> parameters;
  bool has_query = false;
  bool has_filename = false;
};

std::string json_metadata(const std::string &file);
std::string text_metadata(const std::string &file);
std::string code_metadata(const std::string &file);
std::string tsv_metadata(const std::string &file,
                         const std::string &separator, bool header,
                         const std::vector<std::string> &headings,
                         const std::vector<std::string> &features);
std::string forager2json(const std::string &name, const std::string &tag,
                         const std::string &query,
                         const std::map<std::string, std::string> &parameters);
std::string forager_file2json(const std::string &filename,
                              const std::string &name,
                              const std::string &tag);
bool json2forager(const std::string &json, ForagerMetadata *metadata,
                  std::string *error = nullptr);
bool json2forager(const std::string &json, std::string *name, std::string *tag,
                  std::map<std::string, std::string> *parameters,
                  std::string *error = nullptr);
bool json2file(const std::string &json, std::string *type,
               std::string *filename, std::string *error = nullptr);

} // namespace meadowlark
} // namespace cottontail

#endif // COTTONTAIL_MEADOWLARK_METADATA_H_
