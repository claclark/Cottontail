#ifndef COTTONTAIL_SRC_RANKING_H_
#define COTTONTAIL_SRC_RANKING_H_

// Information retrieval ranking and other related algorithms

#include <map>
#include <memory>
#include <vector>

#include "src/core.h"
#include "src/warren.h"

namespace cottontail {

class RankingResult {
public:
  RankingResult(addr p, addr q, fval score)
      : p_(p), q_(q), container_p_(p), container_q_(q), score_(score){};
  RankingResult(addr p, addr q, addr container_p, addr container_q, fval score)
      : p_(p), q_(q), container_p_(container_p), container_q_(container_q),
        score_(score){};
  RankingResult() = default;
  RankingResult(const RankingResult &) = default;
  RankingResult &operator=(const RankingResult &rhs) = default;
  inline addr p() const { return p_; }
  inline addr q() const { return q_; }
  inline addr container_p() const { return container_p_; }
  inline addr container_q() const { return container_q_; }
  inline fval score() const { return score_; }
  inline void container(addr container_p, addr container_q) {
    if (container_p <= p_ && container_q >= q_) {
      container_p_ = container_p;
      container_q_ = container_q;
    }
  }

private:
  addr p_, q_, container_p_, container_q_;
  fval score_;
};

// Reciprocal rank fusion based on:
// Gordon V. Cormack, Charles L A Clarke, and Stefan Buettcher. 2009. Reciprocal
// rank fusion outperforms condorcet and individual rank learning methods.
// 32nd ACM SIGIR. http://dx.doi.org/10.1145/1571941.1572114
std::vector<RankingResult>
rrf_fusion(const std::vector<std::vector<RankingResult>> &rankings);

// Ranking with tiered Boolean queries, based on:
// C. L. A. Clarke, G. V. Cormack, F. J. Burkowski. 1995.
// Shortest Substring Ranking (MultiText Experiments for TREC-4).
// https://trec.nist.gov/pubs/trec4/papers/uwaterloo.ps.gz
std::vector<RankingResult> tiered_ranking(
    std::shared_ptr<Warren> warren, const std::vector<std::string> &tiers,
    const std::string &container, const std::map<std::string, fval> &parameters,
    size_t depth = 1000);

inline std::vector<RankingResult>
tiered_ranking(std::shared_ptr<Warren> warren,
               const std::vector<std::string> &tiers,
               const std::string &container, size_t depth = 1000) {
  std::map<std::string, fval> parameters;
  return tiered_ranking(warren, tiers, container, parameters, depth);
}

// Ranking with auto-generated tiered Boolean queries, vaguely based on:
// Deriving Very Short Queries for High Precision and Recall.
// G.V. Cormack, C.R. Palmer, M. Van Biesbrouck, and C.L.A. Clarke
// TREC-7. 1998. https://trec.nist.gov/pubs/trec7/t7_proceedings.html
// and
// G. V. Cormack, C. L. A. Clarke, C. R. Palmer, and D. I. E. Kisman.
// Fast Automatic Passage Ranking.
// TREC-8. 1999. https://trec.nist.gov/pubs/trec8/t8_proceedings.html
std::vector<RankingResult>
tiered_ranking(std::shared_ptr<Warren> warren, const std::string &query,
               const std::string &container,
               const std::map<std::string, fval> &parameters,
               size_t depth = 1000);

inline std::vector<RankingResult> tiered_ranking(std::shared_ptr<Warren> warren,
                                                 const std::string &query,
                                                 const std::string &container,
                                                 size_t depth = 1000) {
  std::map<std::string, fval> parameters;
  return tiered_ranking(warren, query, container, parameters, depth);
}

std::vector<std::string>
build_tiers(std::shared_ptr<Warren> warren, const std::string &query,
            const std::map<std::string, fval> &parameters);

// Ranking with combinational GCL queries based on:
// Charles L. A. Clarke and Gordon V. Cormack. 2000.
// Shortest-substring retrieval and ranking.
// ACM Transactions on Information Systems 18(1):44-78.
// DOI=http://dx.doi.org/10.1145/333135.333137
std::vector<RankingResult>
ssr_ranking(std::shared_ptr<Warren> warren, const std::string &gcl,
            const std::string &container,
            const std::map<std::string, fval> &parameters, size_t depth = 1000);

inline std::vector<RankingResult> ssr_ranking(std::shared_ptr<Warren> warren,
                                              const std::string &gcl,
                                              const std::string &container,
                                              size_t depth = 1000) {
  std::map<std::string, fval> parameters;
  return ssr_ranking(warren, gcl, container, parameters, depth);
}

// Passage ranking based on:
// Charles L. A. Clarke and Egidio L. Terra. 2004.
// Approximating the top-m passages in a parallel question answering system.
// CIKM 2004.  http://dx.doi.org/10.1145/1031171.1031259
std::vector<RankingResult> icover_ranking(std::shared_ptr<Warren> warren,
                                          const std::string &query,
                                          const std::string &container,
                                          size_t depth = 1000);

// End-to-end statistical question answering based on:
// Charles L. A. Clarke, Gordon V. Cormack, Thomas R. Lynam and Egidio L. Terra.
// Question Answering by Passage Selection.
// In Advances in Open Domain Question Answering, Tomek Strzalkowski (editor),
// Springer, 2006
std::vector<std::string> qap(std::shared_ptr<Warren> warren,
                             const std::string &query,
                             const std::string &container,
                             const std::map<std::string, fval> &parameters);

// Feedback for question answering purposes
std::vector<std::string>
qa_prf(std::shared_ptr<Warren> warren,
       const std::vector<RankingResult> &ranking,
       const std::map<std::string, fval> &parameters,
       std::map<std::string, fval> *weighted_query = nullptr);

inline std::vector<std::string>
qa_prf(std::shared_ptr<Warren> warren,
       const std::vector<RankingResult> &ranking) {
  std::map<std::string, fval> parameters;
  return qa_prf(warren, ranking, parameters);
}

// Pseudo-relevance feedback backed on KL-Divergence. Designed for use
// with short passages, mostly for question answering purposes.
std::vector<std::string>
kld_prf(std::shared_ptr<Warren> warren,
        const std::vector<RankingResult> &ranking,
        const std::map<std::string, fval> &parameters,
        std::map<std::string, fval> *weighted_query = nullptr);

inline std::vector<std::string>
kld_prf(std::shared_ptr<Warren> warren,
        const std::vector<RankingResult> &ranking) {
  std::map<std::string, fval> parameters;
  return kld_prf(warren, ranking, parameters);
}

// Generate term frequency annotations over a range.
bool tf_annotations(std::shared_ptr<Warren> warren,
                    std::string *error = nullptr, addr start = minfinity,
                    addr end = maxfinity);

// Generate term frequency and document frequency annotations.
bool tf_df_annotations(std::shared_ptr<Warren> warren,
                       std::string *error = nullptr);

// Generate annotations for the TF-IDF ranking and pseudo-relevance
// feedback methods below.
bool tf_idf_annotations(std::shared_ptr<Warren> warren,
                        std::string *error = nullptr,
                        bool include_unstemmed = true, bool include_idf = true,
                        bool include_rsj = true);

// Pseudo-relevance feedback based on the description of RM3 in
// Jeffery Dalton, Laura Dietz, James Allan
// Entity Query Feature Expansion using Knowledge Base Links
// SIGIR 2014. See section 2.4
std::vector<std::string>
rm3_prf(std::shared_ptr<Warren> warren,
        const std::vector<RankingResult> &ranking,
        const std::map<std::string, fval> &parameters,
        std::map<std::string, fval> *weighted_query = nullptr);

inline std::vector<std::string>
rm3_prf(std::shared_ptr<Warren> warren,
        const std::vector<RankingResult> &ranking) {
  std::map<std::string, fval> parameters;
  return rm3_prf(warren, ranking, parameters);
}

// Pseudo-relevance feedback based on Robertson Sp{\"a}rck Jones weights.
// Depends on the existance of these weights in the warren
// See tf_idf_annotations above.
std::vector<std::string>
rsj_prf(std::shared_ptr<Warren> warren,
        const std::vector<RankingResult> &ranking,
        const std::map<std::string, fval> &parameters,
        std::map<std::string, fval> *weighted_query = nullptr);

inline std::vector<std::string>
rsj_prf(std::shared_ptr<Warren> warren,
        const std::vector<RankingResult> &ranking) {
  std::map<std::string, fval> parameters;
  return rsj_prf(warren, ranking, parameters);
}

// Language modeling with Dirichlet smoothing.
// Index must contain appropriate annotations.
std::vector<RankingResult>
lmd_ranking(std::shared_ptr<Warren> warren,
            const std::map<std::string, fval> &query,
            const std::map<std::string, fval> &parameters);

std::vector<RankingResult>
lmd_ranking(std::shared_ptr<Warren> warren, const std::string &query,
            const std::map<std::string, fval> &parameters);

std::vector<RankingResult> lmd_ranking(std::shared_ptr<Warren> warren,
                                       const std::string &query);

// Standard BM25.
// Index must contain appropriate annotations.
std::vector<RankingResult>
bm25_ranking(std::shared_ptr<Warren> warren,
             const std::map<std::string, fval> &query,
             const std::map<std::string, fval> &parameters);

std::vector<RankingResult>
bm25_ranking(std::shared_ptr<Warren> warren, const std::string &query,
             const std::map<std::string, fval> &parameters);

std::vector<RankingResult> bm25_ranking(std::shared_ptr<Warren> warren,
                                        const std::string &query);

// Random ranker.
std::vector<RankingResult> random_ranking(std::shared_ptr<Warren> warren,
                                          const std::string &container,
                                          size_t depth = 1000);
// Dot product
std::vector<RankingResult>
product_ranking(std::shared_ptr<Warren> warren,
                const std::map<std::string, fval> &query,
                const std::map<std::string, fval> &parameters,
                const std::string &tag, bool convert);

std::vector<RankingResult>
product_ranking(std::shared_ptr<Warren> warren, const std::string &query,
                const std::map<std::string, fval> &parameters,
                const std::string &tag, bool convert);

std::vector<RankingResult> product_ranking(std::shared_ptr<Warren> warren,
                                           const std::string &query,
                                           const std::string &tag,
                                           bool convert);

} // namespace cottontail

#endif // COTTONTAIL_SRC_RANKING_H_
