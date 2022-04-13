#ifndef COTTONTAIL_SRC_EVAL_H
#define COTTONTAIL_SRC_EVAL_H

#include <map>
#include <memory>
#include <string>

#include "src/core.h"
#include "src/ranking.h"
#include "src/warren.h"

namespace cottontail {
std::string trec_docno(const std::string &text);
bool load_trec_qrels(std::shared_ptr<Warren> warren,
                     const std::string &filename,
                     std::map<std::string, std::map<addr, fval>> *qrels,
                     std::string *error,
                     bool microsoft_interpretation_of_grades = true);
bool load_trec_qrels(std::shared_ptr<Warren> warren,
                     const std::string &filename,
                     std::map<std::string, std::map<addr, fval>> *qrels,
                     std::map<std::string, std::map<addr, addr>> *lengths,
                     std::map<std::string, std::map<addr, std::string>> *docnos,
                     std::string *error,
                     bool microsoft_interpretation_of_grades = true);

void eval(const std::map<addr, fval> &qrels,
          const std::vector<RankingResult> &results,
          std::map<std::string, fval> *metrics);

void med(const std::vector<RankingResult> &one,
         const std::vector<RankingResult> &two,
         std::map<std::string, fval> *metrics);

void mean(const std::map<std::string, fval> &metrics,
          std::map<std::string, fval> *mean_metrics);
}
#endif // COTTONTAIL_SRC_EVAL_H
