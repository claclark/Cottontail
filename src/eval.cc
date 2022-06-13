#include "src/eval.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <vector>

namespace cottontail {

std::string trec_docno(const std::string &text) {
  std::regex docno_start("<DOCNO>");
  std::regex docno_end("</DOCNO>");
  std::regex leading("^\\s*");
  std::regex trailing("\\s*$");
  std::string docno = std::regex_replace(text, docno_start, "");
  docno = std::regex_replace(docno, docno_end, "");
  docno = std::regex_replace(docno, leading, "");
  docno = std::regex_replace(docno, trailing, "");
  return docno;
}

namespace {
bool translate_docno(std::shared_ptr<Warren> warren, const std::string &docno,
                     addr *p, addr *q, std::string *error) {
  std::string container_query = warren->default_container();
  if (container_query == "") {
    safe_set(error) = "warren has no rankable items";
    return false;
  }
  std::string identifier_key = "id";
  std::string identifier_query;
  if (!warren->get_parameter(identifier_key, &identifier_query, error))
    return false;
  if (identifier_query == "") {
    safe_set(error) = "rankable items in warren don't have identifiers";
    return false;
  }
  // (>> container_query (>> identifier_query "docno"))
  std::string query;
  query += "(>> ";
  query += container_query;
  query += " (>> ";
  query += identifier_query;
  query += " \"";
  query += docno;
  query += "\"))";
  std::unique_ptr<Hopper> hopper = warren->hopper_from_gcl(query, error);
  if (hopper == nullptr)
    return false;
  hopper->tau(minfinity + 1, p, q);
  if (*p == maxfinity) {
    safe_set(error) = "item not found in warren: " + docno;
    return false;
  }
  return true;
}
} // namespace

bool load_trec_qrels(std::shared_ptr<Warren> warren,
                     const std::string &filename,
                     std::map<std::string, std::map<addr, fval>> *qrels,
                     std::string *error,
                     bool microsoft_interpretation_of_grades) {
  return load_trec_qrels(warren, filename, qrels, nullptr, nullptr, error,
                         microsoft_interpretation_of_grades);
}

bool load_trec_qrels(std::shared_ptr<Warren> warren,
                     const std::string &filename,
                     std::map<std::string, std::map<addr, fval>> *qrels,
                     std::map<std::string, std::map<addr, addr>> *lengths,
                     std::map<std::string, std::map<addr, std::string>> *docnos,
                     std::string *error,
                     bool microsoft_interpretation_of_grades) {
  std::ifstream qrelsf(filename);
  if (qrelsf.fail()) {
    safe_set(error) = "cannot open qrels file \"" + filename + "\"";
    return false;
  }
  std::regex ws_re("\\s+");
  std::string line;
  fval max_level = 0.0;
  while (std::getline(qrelsf, line)) {
    std::vector<std::string> field{
        std::sregex_token_iterator(line.begin(), line.end(), ws_re, -1), {}};
    if (field.size() != 4) {
      safe_set(error) = "format error in qrels file \"" + filename + "\"";
      return false;
    }
    fval level = std::max(0.0, atof(field[3].c_str()));
    max_level = std::max(level, max_level);
    addr p, q;
    if (!translate_docno(warren, field[2], &p, &q, error))
      return false;
    if (qrels != nullptr)
      (*qrels)[field[0]][p] = level;
    if (lengths != nullptr)
      (*lengths)[field[0]][p] = q - p + 1;
    if (docnos != nullptr)
      (*docnos)[field[0]][p] = field[2];
  }
  if (qrels == nullptr)
    return true;
  for (auto &&topic : (*qrels))
    for (auto &&docno : topic.second)
      if (microsoft_interpretation_of_grades)
        (*qrels)[topic.first][docno.first] =
            (std::pow(2.0, docno.second) - 1.0) / std::pow(2.0, max_level);
      else
        (*qrels)[topic.first][docno.first] /= max_level;
  return true;
}

namespace {
fval count_binary_rels(const std::map<addr, fval> &qrels) {
  fval count = 0;
  for (auto &&qrel : qrels)
    if (qrel.second > 0.0)
      count++;
  return count;
}

void trec_eval(const std::map<addr, fval> &qrels,
               const std::vector<RankingResult> &results,
               std::map<std::string, fval> *metrics) {
  fval rels_possible = count_binary_rels(qrels);
  fval rels_seen = 0.0, ap = 0.0, p05 = 0.0, p10 = 0.0, p20 = 0.0, rr = 0.0;
  if (rels_possible > 0.0 && results.size() > 0) {
    size_t rank;
    for (rank = 1; rank <= results.size(); rank++) {
      auto qrel = qrels.find(results[rank - 1].p());
      fval rval;
      if (qrel == qrels.end())
        rval = 0.0;
      else
        rval = qrel->second;
      if (rval > 0.0) {
        rels_seen++;
        ap += rels_seen / rank;
        if (rr == 0.0)
          rr = 1.0 / rank;
      }
      if (rank == 5)
        p05 = rels_seen / 5.0;
      if (rank == 10)
        p10 = rels_seen / 10.0;
      if (rank == 20)
        p20 = rels_seen / 20.0;
    }
    if (rank <= 5)
      p05 = rels_seen / 5.0;
    if (rank <= 10)
      p10 = rels_seen / 10.0;
    if (rank <= 20)
      p20 = rels_seen / 20.0;
    ap /= rels_possible;
  }
  (*metrics)["ap"] = ap;
  (*metrics)["rr"] = ap;
  (*metrics)["p05"] = p05;
  (*metrics)["p10"] = p10;
  (*metrics)["p20"] = p20;
  (*metrics)["ret"] = results.size();
  (*metrics)["rel"] = rels_possible;
  (*metrics)["rel-ret"] = rels_seen;
  if ((*metrics)["ret"] > 0.0)
    (*metrics)["precision"] = (*metrics)["rel-ret"] / (*metrics)["ret"];
  else
    (*metrics)["precision"] = 0.0;
  if ((*metrics)["rel"] > 0.0)
    (*metrics)["recall"] = (*metrics)["rel-ret"] / (*metrics)["rel"];
  else
    (*metrics)["recall"] = 0.0;
}

void ndcg_eval(const std::map<addr, fval> &qrels,
               const std::vector<RankingResult> &results,
               std::map<std::string, fval> *metrics) {
  std::vector<fval> ideal;
  for (auto &qrel : qrels)
    ideal.push_back(qrel.second);
  std::sort(ideal.begin(), ideal.end(),
            [](const auto &a, const auto &b) -> bool { return a > b; });
  fval ideal_score = 0.0, ideal_05 = 0.0, ideal_10 = 0.0, ideal_20 = 0.0;
  for (size_t i = 0; i < ideal.size(); i++) {
    ideal_score += ideal[i] / std::log(i + 2.0);
    if (i == 4)
      ideal_05 = ideal_score;
    if (i == 9)
      ideal_10 = ideal_score;
    if (i == 19)
      ideal_20 = ideal_score;
  }
  if (ideal_score == 0.0) {
    (*metrics)["dcg"] = (*metrics)["dcg05"] = (*metrics)["dcg10"] =
        (*metrics)["dcg20"] = 0.0;
    (*metrics)["ndcg"] = (*metrics)["ndcg05"] = (*metrics)["ndcg10"] =
        (*metrics)["ndcg20"] = 0.0;
    return;
  }
  if (ideal.size() < 5)
    ideal_05 = ideal_score;
  if (ideal.size() < 10)
    ideal_10 = ideal_score;
  if (ideal.size() < 20)
    ideal_20 = ideal_score;
  fval actual_score = 0.0, actual_05 = 0.0, actual_10 = 0.0, actual_20 = 0.0;
  for (size_t i = 0; i < results.size(); i++) {
    auto qrel = qrels.find(results[i].p());
    fval rval;
    if (qrel == qrels.end())
      rval = 0.0;
    else
      rval = qrel->second;
    actual_score += rval / std::log(i + 2.0);
    if (i == 4)
      actual_05 = actual_score;
    if (i == 9)
      actual_10 = actual_score;
    if (i == 19)
      actual_20 = actual_score;
  }
  if (results.size() < 5)
    actual_05 = actual_score;
  if (results.size() < 10)
    actual_10 = actual_score;
  if (results.size() < 20)
    actual_20 = actual_score;
  (*metrics)["dcg"] = std::log(2.0) * actual_score;
  (*metrics)["dcg05"] = std::log(2.0) * actual_05;
  (*metrics)["dcg10"] = std::log(2.0) * actual_10;
  (*metrics)["dcg20"] = std::log(2.0) * actual_20;
  (*metrics)["ndcg"] = actual_score / ideal_score;
  (*metrics)["ndcg05"] = actual_05 / ideal_05;
  (*metrics)["ndcg10"] = actual_10 / ideal_10;
  (*metrics)["ndcg20"] = actual_20 / ideal_20;
}

void rbp_eval(const std::map<addr, fval> &qrels,
              const std::vector<RankingResult> &results,
              std::map<std::string, fval> *metrics) {
  fval rbp50 = 0.0, rbp95 = 0.0;
  fval prb50 = 1.0, prb95 = 1.0;
  for (size_t i = 0; i < results.size(); i++) {
    auto qrel = qrels.find(results[i].p());
    fval rval;
    if (qrel == qrels.end())
      rval = 0.0;
    else
      rval = qrel->second;
    rbp50 += rval * prb50;
    prb50 *= 0.50;
    rbp95 += rval * prb95;
    prb95 *= 0.95;
  }
  (*metrics)["rbp50"] = (1.0 - 0.50) * rbp50;
  (*metrics)["rbp95"] = (1.0 - 0.95) * rbp95;
}

// Time-biased gain based on:
// Mark D. Smucker and Charles L. A. Clarke
// Time-based calibration of effectiveness measures. SIGIR 2012.
// Assumes that the ranked list does not contain (effective) duplicates.
// Note that this metric assumes binary relevance judgments
void tbg_eval(const std::map<addr, fval> &qrels,
              const std::vector<RankingResult> &results,
              std::map<std::string, fval> *metrics) {
  // User behaviour simulation constants (see table 1)
  const fval PCR1 = 0.64; // Probability of a click on a relevant summary
  const fval PCR0 = 0.39; // Probability of a click on a non-relevant summary
  const fval PSR1 = 0.77; // Probability of a "save" of a relevant document
  const fval TS = 4.4;    // Time to evaluate a summary (seconds)
  const fval TDA = 0.018; // Length-dependent reading time (seconds/token)
  const fval TDB = 7.8;   // Length-independent reading time (seconds)
  const fval H = 224.0;   // Decay half-life (see Eq. 6 and section 5.2)

  fval tbg = 0.0;
  fval time_spent = 0.0;
  for (size_t rank = 1; rank <= results.size(); rank++) {
    fval decay = std::exp(-time_spent * std::log(2.0) / H);
    fval click_probability;
    auto qrel = qrels.find(results[rank - 1].p());
    if (qrel == qrels.end() || qrel->second == 0.0) {
      click_probability = PCR0;
    } else {
      tbg += PCR1 * PSR1 * decay;
      click_probability = PCR1;
    }
    fval document_length = results[rank - 1].q() - results[rank - 1].p() + 1.0;
    fval reading_time = TDA * document_length + TDB;
    time_spent += TS + reading_time * click_probability;
  }
  (*metrics)["tbg"] = tbg;
}
} // namespace

void eval(const std::map<addr, fval> &qrels,
          const std::vector<RankingResult> &results,
          std::map<std::string, fval> *metrics) {
  trec_eval(qrels, results, metrics);
  ndcg_eval(qrels, results, metrics);
  rbp_eval(qrels, results, metrics);
  tbg_eval(qrels, results, metrics);
}

namespace {
std::map<addr, fval> med_qrels(const std::vector<RankingResult> &one,
                               const std::vector<RankingResult> &two) {
  std::map<addr, fval> qrels;
  size_t i;
  for (i = 0; i < std::min(one.size(), two.size()); i++) {
    auto qrel1 = qrels.find(one[i].p());
    if (qrel1 == qrels.end())
      qrels[one[i].p()] = 1.0;
    auto qrel2 = qrels.find(two[i].p());
    if (qrel2 == qrels.end())
      qrels[two[i].p()] = 0.0;
  }
  for (; i < one.size(); i++) {
    auto qrel1 = qrels.find(one[i].p());
    if (qrel1 == qrels.end())
      qrels[one[i].p()] = 1.0;
  }
  return qrels;
}

void med_half(const std::vector<RankingResult> &one,
              const std::vector<RankingResult> &two,
              std::map<std::string, fval> *metrics) {
  std::map<addr, fval> qrels = med_qrels(one, two);
  std::map<std::string, fval> metrics1;
  std::map<std::string, fval> metrics2;
  ndcg_eval(qrels, one, &metrics1);
  rbp_eval(qrels, one, &metrics1);
  ndcg_eval(qrels, two, &metrics2);
  rbp_eval(qrels, two, &metrics2);
  (*metrics)["med-dcg"] = metrics1["dcg"] - metrics2["dcg"];
  (*metrics)["med-rbp50"] = metrics1["rbp50"] - metrics2["rbp50"];
  (*metrics)["med-rbp95"] = metrics1["rbp95"] - metrics2["rbp95"];
}

fval med_fake_ideal_dcg(size_t depth) {
  fval score = 0.0;
  for (size_t i = 0; i < depth; i++)
    score += 1.0 / std::log(i + 2.0);
  return std::log(2.0) * score;
}
} // namespace

// Rank similarity based on:
// Luchen Tan and Charles L. A. Clarke. A Family of Rank Similarity Measures
// Based on Maximized Effectiveness Difference. IEEE Transactions on Knowledge
// and Data Engineeering 27(11): 2865-2877 (2015)
// Note that this "optimistic qrels" approach only works for some metrics,
// including DCG (sort of) and RBP, but not AP. NDCG requires some hackery.
void med(const std::vector<RankingResult> &one,
         const std::vector<RankingResult> &two,
         std::map<std::string, fval> *metrics) {
  std::map<std::string, fval> metrics1;
  med_half(one, two, &metrics1);
  std::map<std::string, fval> metrics2;
  med_half(two, one, &metrics2);
  (*metrics)["med-dcg"] = std::max(metrics1["med-dcg"], metrics2["med-dcg"]);
  (*metrics)["med-rbp50"] =
      std::max(metrics1["med-rbp50"], metrics2["med-rbp50"]);
  (*metrics)["med-rbp95"] =
      std::max(metrics1["med-rbp95"], metrics2["med-rbp95"]);
  (*metrics)["med-ndcg"] = (*metrics)["med-dcg"] /
                           med_fake_ideal_dcg(std::max(one.size(), two.size()));
}

void mean(const std::map<std::string, fval> &metrics,
          std::map<std::string, fval> *mean_metrics) {
  if ((*mean_metrics).size() == 0) {
    (*mean_metrics) = metrics;
    (*mean_metrics)["n"] = 1.0;
  } else if ((*mean_metrics).find("n") == (*mean_metrics).end()) {
    for (auto &&metric : (*mean_metrics))
      if (metric.first != "n") {
        if (metrics.find(metric.first) == metrics.end()) {
          (*mean_metrics).erase(metric.first);
        } else {
          (*mean_metrics)[metric.first] += metrics.at(metric.first);
          (*mean_metrics)[metric.first] /= 2.0;
        }
      }
    (*mean_metrics)["n"] = 2.0;
  } else {
    fval n = (*mean_metrics)["n"];
    assert(n > 0.0);
    for (auto &&metric : (*mean_metrics))
      if (metric.first != "n") {
        if (metrics.find(metric.first) == metrics.end()) {
          (*mean_metrics).erase(metric.first);
        } else {
          (*mean_metrics)[metric.first] *= n;
          (*mean_metrics)[metric.first] += metrics.at(metric.first);
          (*mean_metrics)[metric.first] /= (n + 1.0);
        }
      }
    (*mean_metrics)["n"]++;
  }
}
} // namespace cottontail
