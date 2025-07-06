#include "src/meadowlark/meadowlark.h"

#include <memory>
#include <string>

#include "src/json.h"
#include "src/warren.h"

namespace cottontail {
namespace meadowlark {

bool append_path(std::shared_ptr<Warren> warren, const std::string &filename,
                 addr *path_feature, std::string *error) {
  std::string path;
  if (filename.find("/") != std::string::npos)
    path = filename;
  else
    path = "./" + filename;
  if (!warren->transaction(error))
    return false;
  addr p, q;
  if (!warren->appender()->append(json_encode(path), &p, &q, error)) {
    warren->abort();
    return false;
  }
  if (!warren->annotator()->annotate(warren->featurizer()->featurize("/"), p, q, error)) {
    warren->abort();
    return false;
  }
  if (!warren->ready(error)) {
    warren->abort();
    return false;
  }
  warren->commit();
  *path_feature = warren->featurizer()->featurize(path);
  return true;
}

} // namespace meadowlark
} // namespace cottontail
