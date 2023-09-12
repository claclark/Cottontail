#ifndef COTTONTAIL_SRC_SIMPLE_H_
#define COTTONTAIL_SRC_SIMPLE_H_

// Common definitions for simple search structures in a single flat directory.

#include <map>
#include <string>

#include "src/dna.h"
#include "src/core.h"
#include "src/working.h"

namespace cottontail {

// Some of these file names are traditional.
const std::string RAW_NAME = "raw"; // raw text (compressed)
const std::string MAP_NAME = "map"; // chunk map for raw text io
const std::string TXT_NAME = "txt"; // token map for text
const std::string IDX_NAME = "idx"; // dictionary for inverted index
const std::string PST_NAME = "pst"; // postings lists (compressed)

static const addr TEXT_COMPRESSOR_CHUNK_SIZE = 1024 * 1024;
static const addr TXT_BLOCKING = 1024;

// Text compressor must not trash its input
const std::string TEXT_COMPRESSOR_NAME = "zlib";
const std::string TEXT_COMPRESSOR_RECIPE = "";

// Posting compressor can trash its input
const std::string POSTING_COMPRESSOR_NAME = "post";
const std::string POSTING_COMPRESSOR_RECIPE = "";

// Feature value compressor can trash its input
const std::string FVALUE_COMPRESSOR_NAME = "zlib";
const std::string FVALUE_COMPRESSOR_RECIPE = "";

// File formats used only by Simple classes.
struct IdxRecord {
  IdxRecord() = default;
  IdxRecord(addr feature, addr end) : feature(feature), end(end){};
  addr feature, end;
};
struct PstRecord {
  PstRecord() = default;
  PstRecord(addr feature, addr n, addr pst, addr qst, addr fst)
      : feature(feature), n(n), pst(pst), qst(qst), fst(fst){};
  addr feature, n, pst, qst, fst;
};
struct TokRecord {
  TokRecord() = default;
  TokRecord(addr feature, addr pq) : feature(feature), pq(pq){};
  addr feature, pq;
};
struct TxtRecord {
  TxtRecord() = default;
  TxtRecord(addr pq, addr start, addr end) : pq(pq), start(start), end(end){};
  addr pq, start, end;
};

} // namespace cottontail

#endif // COTTONTAIL_SRC_SIMPLE_H_
