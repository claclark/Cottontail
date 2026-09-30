#ifndef COTTONTAIL_SRC_SPLADE_H_
#define COTTONTAIL_SRC_SPLADE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "src/core.h"
#include "src/warren.h"

namespace cottontail {

// Exact document-at-a-time ranking of JSON vector annotations. Parameters:
// container (GCL, default ":"), id (GCL, default ":docid:"),
// prefix (default ":splade_vector:"), depth (default "1000").
// A query label is looked up as prefix + label + ":", without tokenization.
// Containers must not overlap; each vector label has at most one single-position
// numeric annotation per container, as in ordinary JSON vector records.
// Weights must be finite and nonnegative; only positive-scoring matches rank.
// Results contain translated identifier intervals, as in trec; trec_docno can
// format these for TREC output. Equal scores prefer earlier container starts.
// The caller's read epoch is preserved. Time is the longest worker ranking
// time in milliseconds, excluding worker setup, as in trec.
bool splade(
    std::shared_ptr<Warren> warren,
    const std::map<std::string, std::map<std::string, fval>> &queries,
    const std::map<std::string, std::string> &parameters,
    std::map<std::string, std::vector<std::string>> *results,
    std::string *error = nullptr, size_t threads = 0, addr *time = nullptr);

inline bool splade(
    std::shared_ptr<Warren> warren,
    const std::map<std::string, std::map<std::string, fval>> &queries,
    std::map<std::string, std::vector<std::string>> *results,
    std::string *error = nullptr, size_t threads = 0, addr *time = nullptr) {
  std::map<std::string, std::string> parameters;
  return splade(warren, queries, parameters, results, error, threads, time);
}

} // namespace cottontail
#endif // COTTONTAIL_SRC_SPLADE_H_
