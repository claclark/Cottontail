#include "src/ranking.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "src/cottontail.h"
#include "src/gcl.h"
#include "src/hopper.h"
#include "src/parameters.h"
#include "src/parse.h"
#include "src/stats.h"
#include "src/tagging_featurizer.h"

namespace cottontail {

namespace {
fval ranking_parameter(std::string ranker_name, std::string parameter_name,
                       const std::map<std::string, fval> &parameters) {
  std::string full_parameter_name = ranker_name + ":" + parameter_name;
  if (parameters.find(full_parameter_name) != parameters.end())
    return parameters.at(full_parameter_name);
  if (parameters.find(parameter_name) != parameters.end())
    return parameters.at(parameter_name);
  std::map<std::string, fval> defaults =
      Parameters::from_ranker_name(ranker_name)->defaults();
  if (defaults.find(full_parameter_name) != defaults.end())
    return defaults[full_parameter_name];
  assert(defaults.find(parameter_name) != defaults.end());
  return defaults[parameter_name];
}
} // namespace

// Reciprocal rank fusion based on:
// Gordon V. Cormack, Charles L A Clarke, and Stefan Buettcher. 2009. Reciprocal
// rank fusion outperforms condorcet and individual rank learning methods.
// 32nd ACM SIGIR. http://dx.doi.org/10.1145/1571941.1572114
std::vector<RankingResult>
rrf_fusion(const std::vector<std::vector<RankingResult>> &rankings) {
  const fval K = 60.0; // experience says this is pointless to tune
  std::vector<RankingResult> fused;
  std::map<addr, fval> scores;
  for (auto &ranking : rankings)
    for (size_t i = 0; i < ranking.size(); i++)
      if (scores.find(ranking[i].p()) == scores.end())
        scores[ranking[i].p()] = 1.0 / (K + i + 1.0);
      else
        scores[ranking[i].p()] += 1.0 / (K + i + 1.0);
  std::set<addr> seen;
  for (auto &ranking : rankings)
    for (auto &result : ranking)
      if (seen.find(result.p()) == seen.end()) {
        seen.insert(result.p());
        fused.emplace_back(result.p(), result.q(), result.container_p(),
                           result.container_q(), scores[result.p()]);
      }
  std::sort(fused.begin(), fused.end(),
            [](const auto &a, const auto &b) -> bool {
              return a.score() > b.score();
            });
  return fused;
}

namespace {

// templates for convenience ranker prototypes

template <std::vector<RankingResult> ALGORITHM(
    std::shared_ptr<Warren> warren, const std::map<std::string, fval> &query,
    const std::map<std::string, fval> &parameters)>
std::vector<RankingResult>
ranking(std::shared_ptr<Warren> warren, const std::string &query,
        const std::map<std::string, fval> &parameters) {
  std::vector<std::string> terms = warren->tokenizer()->split(query);
  std::map<std::string, fval> weighted_query;
  for (auto &term : terms)
    weighted_query[term] = 1.0;
  return ALGORITHM(warren, weighted_query, parameters);
}

template <std::vector<RankingResult> ALGORITHM(
    std::shared_ptr<Warren> warren, const std::string &query,
    const std::map<std::string, fval> &parameters)>
std::vector<RankingResult> ranking(std::shared_ptr<Warren> warren,
                                   const std::string &query) {
  const std::map<std::string, fval> parameters;
  return ALGORITHM(warren, query, parameters);
}
} // namespace

std::vector<std::string>
build_tiers(std::shared_ptr<Warren> warren, const std::string &query,
            const std::map<std::string, fval> &parameters) {
  fval delta = ranking_parameter("tiered", "delta", parameters);
  std::vector<std::string> tiers;
  std::vector<std::string> raw_terms = warren->tokenizer()->split(query);
  if (raw_terms.size() == 0)
    return tiers;
  std::set<std::string> seen;
  std::vector<std::string> terms;
  for (auto &&term : raw_terms)
    if (seen.find(term) == seen.end()) {
      seen.insert(term);
      terms.push_back(term);
    }
  std::map<std::string, fval> weights;
  fval vocab = warren->idx()->vocab();
  fval tokens = warren->txt()->tokens();
  for (auto &&term : terms) {
    addr feature = warren->featurizer()->featurize(term);
    addr count = warren->idx()->count(feature);
    weights[term] = std::log((tokens + vocab) / (count + 1.0));
  }
  std::sort(terms.begin(), terms.end(),
            [&](const std::string &a, const std::string &b) -> bool {
              return weights[a] > weights[b];
            });
  size_t SANE = 16;
  if (terms.size() > SANE)
    terms.erase(terms.begin() + SANE, terms.end());
  size_t n = (0x1 << terms.size()) - 1;
  std::vector<std::string> subqueries;
  for (size_t m = n; m > 0; --m) {
    fval weight = 0.0;
    std::vector<std::string> current;
    size_t j = 0;
    for (size_t i = m; i > 0; i = i >> 1) {
      if (i % 2 == 1) {
        current.push_back(terms[j]);
        weight += weights[terms[j]];
      }
      j++;
    }
    std::string subquery;
    if (current.size() > 1) {
      subquery = "(^";
      for (auto &&term : current)
        subquery += (" " + term);
      subquery += " )";
    } else {
      subquery = ("(... " + current[0] + " " + current[0] + ")");
    }
    subqueries.push_back(subquery);
    weights[subquery] = weight;
  }
  std::sort(subqueries.begin(), subqueries.end(),
            [&](const std::string &a, const std::string &b) -> bool {
              return weights[a] > weights[b];
            });
  std::vector<std::string> current;
  fval weight = 0.0;
  for (auto &&subquery : subqueries) {
    if (current.size() == 0) {
      current.push_back(subquery);
      weight = weights[subquery];
    } else if (weight - weights[subquery] < delta) {
      current.push_back(subquery);
    } else if (current.size() > 1) {
      std::string tier = "(+";
      for (auto &&csq : current)
        tier += (" " + csq);
      tier += ")";
      tiers.push_back(tier);
      current.clear();
      current.push_back(subquery);
      weight = weights[subquery];
    } else {
      tiers.push_back(current[0]);
      current.clear();
      current.push_back(subquery);
      weight = weights[subquery];
    }
  }
  if (current.size() > 1) {
    std::string tier = "(+";
    for (auto &&csq : current)
      tier += (" " + csq);
    tier += ")";
    tiers.push_back(tier);
  } else if (current.size() == 1) {
    tiers.push_back(current[0]);
  }
  return tiers;
}

// Ranking with tiered Boolean queries, based on:
// C. L. A. Clarke, G. V. Cormack, F. J. Burkowski. 1995.
// Shortest Substring Ranking (MultiText Experiments for TREC-4).
// https://trec.nist.gov/pubs/trec4/papers/uwaterloo.ps.gz
std::vector<RankingResult>
tiered_ranking(std::shared_ptr<Warren> warren,
               const std::vector<std::string> &tiers,
               const std::string &container,
               const std::map<std::string, fval> &parameters, size_t depth) {
  std::vector<RankingResult> top;
  if (tiers.size() == 0)
    return top;
  std::set<addr> container_seen;
  for (auto &&tier : tiers) {
    std::vector<RankingResult> ranking =
        ssr_ranking(warren, tier, container, depth);
    for (size_t i = 0; i < ranking.size() && top.size() < depth; i++)
      if (container_seen.find(ranking[i].container_p()) ==
          container_seen.end()) {
        container_seen.insert(ranking[i].container_p());
        fval fake_score = depth - top.size();
        top.emplace_back(ranking[i].p(), ranking[i].q(), fake_score);
      }
    if (top.size() == depth)
      break;
  }
  return top;
}

// Ranking with auto-generated tiered Boolean queries, somewhat based on:
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
               const std::map<std::string, fval> &parameters, size_t depth) {
  std::vector<std::string> tiers = build_tiers(warren, query, parameters);
  return tiered_ranking(warren, tiers, container, parameters, depth);
}

namespace {
// Assumes no duplicates. Works for ranking algorithms that compute a score for
// each item in turn.
std::vector<RankingResult>
top_results(const std::vector<RankingResult> &results0,
            const std::vector<RankingResult> &results1, size_t depth) {
  std::vector<RankingResult> combined;
  for (auto &&result : results0)
    combined.push_back(result);
  for (auto &&result : results1)
    combined.push_back(result);
  std::sort(combined.begin(), combined.end(),
            [](const auto &a, const auto &b) -> bool {
              return a.score() > b.score();
            });
  if (combined.size() > depth)
    combined.erase(combined.begin() + depth, combined.end());
  return combined;
}
} // namespace

// Charles L. A. Clarke and Gordon V. Cormack. 2000.
// Shortest-substring retrieval and ranking.
// ACM Transactions on Information Systems 18(1):44-78.
// http://dx.doi.org/10.1145/333135.333137
std::vector<RankingResult>
ssr_ranking(std::shared_ptr<Warren> warren, const std::string &gcl,
            const std::string &container,
            const std::map<std::string, fval> &parameters, size_t depth) {
  fval K = ranking_parameter("ssr", "K", parameters);
  std::vector<RankingResult> top;
  if (depth == 0)
    return top;
  std::string error;
  std::unique_ptr<cottontail::Hopper> hopper =
      warren->hopper_from_gcl(gcl, &error);
  if (hopper == nullptr)
    return top;
  std::unique_ptr<cottontail::Hopper> chopper =
      warren->hopper_from_gcl(container, &error);
  if (chopper == nullptr)
    return top;
  std::vector<RankingResult> current;
  addr p, q, cp, cq;
  hopper->tau(minfinity + 1, &p, &q);
  chopper->rho(q, &cp, &cq);
  fval score = 0.0;
  fval target = 0.0;
  while (p < maxfinity && cq < maxfinity) {
    target = (top.size() == depth ? top[top.size() - 1].score() : 0.0);
    if (p < cp) {
      hopper->tau(cp, &p, &q);
    } else if (q > cq) {
      if (score > target) {
        current.emplace_back(cp, cq, score);
        if (current.size() == depth) {
          top = top_results(top, current, depth);
          current.clear();
        }
      }
      score = 0.0;
      chopper->rho(q, &cp, &cq);
    } else {
      score += 1.0 / (K + q - p);
      hopper->tau(p + 1, &p, &q);
    }
  }
  if (score > target)
    current.emplace_back(cp, cq, score);
  top = top_results(top, current, depth);
  return top;
}

namespace {

// Merges ranking results that may contain duplicate containers.
// Keeps only the top scoring result for each container.
// Containers must form a GC-list.
std::vector<RankingResult>
merge_results(const std::vector<RankingResult> &results0,
              const std::vector<RankingResult> &results1, size_t depth,
              std::vector<RankingResult> *holding) {
  std::vector<RankingResult> combined = results0;
  combined.insert(combined.end(), results1.begin(), results1.end());
  std::sort(combined.begin(), combined.end(),
            [](const auto &a, const auto &b) -> bool {
              return a.score() > b.score();
            });
  std::stable_sort(combined.begin(), combined.end(),
                   [](const auto &a, const auto &b) -> bool {
                     return a.container_p() < b.container_p();
                   });
  std::vector<RankingResult> merged;
  for (auto &result : combined)
    if (merged.empty() || merged.back().container_p() < result.container_p())
      merged.push_back(result);
  std::sort(merged.begin(), merged.end(),
            [](const auto &a, const auto &b) -> bool {
              return a.score() > b.score();
            });
  if (merged.size() > depth)
    merged.erase(merged.begin() + depth, merged.end());
  return merged;
}
} // namespace

// Passage ranking based on:
// Charles L. A. Clarke and Egidio L. Terra.
// Approximating the top-m passages in a parallel question answering system.
// CIKM 2004. http://dx.doi.org/10.1145/1031171.1031259
std::vector<RankingResult> icover_ranking(std::shared_ptr<Warren> warren,
                                          const std::string &query,
                                          const std::string &container,
                                          size_t depth) {
  std::vector<RankingResult> top;
  std::vector<std::string> terms = warren->tokenizer()->split(query);
  if (terms.size() == 0)
    return top;
  std::string error;
  std::shared_ptr<cottontail::gcl::SExpression> expr =
      cottontail::gcl::SExpression::from_string(container, &error);
  if (expr == nullptr)
    return top;
  expr = expr->expand_phrases(warren->tokenizer());
  std::unique_ptr<cottontail::Hopper> chopper =
      expr->to_hopper(warren->featurizer(), warren->idx());
  if (chopper == nullptr)
    return top;
  struct WeightedHopper {
    WeightedHopper(fval weight, addr position, std::unique_ptr<Hopper> hopper)
        : weight(weight), position(position), hopper(std::move(hopper)){};
    fval weight;
    addr position;
    std::unique_ptr<Hopper> hopper;
  };
  std::vector<WeightedHopper> whs;
  fval tokens = warren->txt()->tokens();
  fval vocab = warren->idx()->vocab();
  for (auto &&term : terms) {
    addr feature = warren->featurizer()->featurize(term);
    addr count = warren->idx()->count(feature);
    fval weight = std::log((tokens + vocab) / (count + 1.0));
    whs.emplace_back(weight, minfinity, warren->idx()->hopper(feature));
  }
  fval target = 0.0;
  std::vector<RankingResult> holding;
  for (size_t n = terms.size(); n > 1; --n) {
    target = (top.size() >= depth ? top.back().score() : 0.0);
    std::vector<RankingResult> current;
    if (top.size() >= depth) {
      std::sort(whs.begin(), whs.end(),
                [](const auto &a, const auto &b) -> bool {
                  return a.weight < b.weight;
                });
      while (whs.size() >= n) {
        fval max_possible_score = 0.0;
        for (size_t i = 0; i < n; i++) {
          max_possible_score += whs[i].weight;
        }
        if (max_possible_score <= target)
          whs.erase(whs.begin());
        else
          break;
      }
      if (whs.size() < n)
        return top;
    }
    addr p, q, k = minfinity + 1;
    for (;;) {
      addr ignored;
      for (size_t i = 0; i < whs.size(); i++)
        whs[i].hopper->tau(k, &ignored, &(whs[i].position));
      std::sort(whs.begin(), whs.end(),
                [](const auto &a, const auto &b) -> bool {
                  return a.position < b.position;
                });
      q = whs[n - 1].position;
      if (q == maxfinity)
        break;
      whs[n - 1].hopper->uat(q, &p, &ignored);
      fval score = whs[n - 1].weight;
      for (size_t i = 0; i < n - 1; i++) {
        whs[i].hopper->uat(q, &(whs[i].position), &ignored);
        p = std::min(p, whs[i].position);
        score += whs[i].weight;
      }
      score -= n * std::log(q - p + 1.0);
      addr cp, cq;
      if (score > target) {
        chopper->rho(q, &cp, &cq);
        if (cp <= p) {
          current.emplace_back(p, q, cp, cq, score);
          if (current.size() == depth) {
            top = merge_results(top, current, depth, &holding);
            target = (top.size() >= depth ? top.back().score() : 0.0);
            current.clear();
          }
        }
      }
      k = p + 1;
    }
    if (current.size() > 0) {
      top = merge_results(top, current, depth, &holding);
      target = (top.size() >= depth ? top.back().score() : 0.0);
    }
  }
  if (top.size() >= depth) {
    std::sort(whs.begin(), whs.end(), [](const auto &a, const auto &b) -> bool {
      return a.weight < b.weight;
    });
    while (whs.size() > 0) {
      if (whs[0].weight <= target)
        whs.erase(whs.begin());
      else
        break;
    }
    if (whs.size() == 0)
      return top;
  }
  std::vector<RankingResult> onesies;
  std::sort(whs.begin(), whs.end(), [](const auto &a, const auto &b) -> bool {
    return a.weight > b.weight;
  });
  cottontail::addr k = 0, p, q;
  while (whs.size() > 0 && onesies.size() < depth) {
    whs[0].hopper->tau(k, &p, &q);
    if (q < maxfinity) {
      addr cp, cq;
      chopper->rho(q, &cp, &cq);
      if (cp <= p)
        onesies.emplace_back(p, q, cp, cq, whs[0].weight);
      k = p + 1;
    } else {
      whs.erase(whs.begin());
      k = minfinity + 1;
    }
  }
  top = merge_results(top, onesies, depth, &holding);

  return top;
}

// Feedback for question answering purposes
std::vector<std::string> qa_prf(std::shared_ptr<Warren> warren,
                                const std::vector<RankingResult> &ranking,
                                const std::map<std::string, fval> &parameters,
                                std::map<std::string, fval> *weighted_query) {
  std::vector<std::string> expansion_terms;
  if (ranking.size() == 0)
    return expansion_terms;
  size_t expansions =
      static_cast<size_t>(ranking_parameter("kld", "expansions", parameters));
  size_t depth =
      static_cast<size_t>(ranking_parameter("kld", "depth", parameters));
  addr window =
      static_cast<addr>(ranking_parameter("kld", "window", parameters));
  std::set<cottontail::addr> containers_seen;
  fval txt_tokens = warren->txt()->tokens();
  fval idx_vocab = warren->idx()->vocab();
  std::map<std::string, fval> frequencies;
  std::map<std::string, fval> expectations;
  size_t k = 0;
  for (auto &&result : ranking) {
    if (containers_seen.find(result.container_p()) != containers_seen.end())
      continue;
    containers_seen.insert(result.container_p());
    std::map<std::string, fval> range;
    addr p, q;
    p = result.p() - window;
    if (p < result.container_p())
      p = result.container_p();
    q = result.p() - 1;
    if (q < result.container_p())
      q = result.container_p();
    if (p <= q) {
      std::string text = warren->txt()->translate(p, q);
      std::vector<std::string> tokens = warren->tokenizer()->split(text);
      for (size_t i = 0; i < tokens.size(); i++) {
        fval gap = tokens.size() - i + result.q() - result.p();
        if (range.find(tokens[i]) == range.end())
          range[tokens[i]] = gap;
        else
          range[tokens[i]] = std::min(gap, range[tokens[i]]);
      }
    }
    p = result.p();
    q = result.q();
    if (p <= q) {
      std::string text = warren->txt()->translate(p, q);
      std::vector<std::string> tokens = warren->tokenizer()->split(text);
      for (size_t i = 0; i < tokens.size(); i++) {
        fval gap = q - p + 1;
        if (range.find(tokens[i]) == range.end())
          range[tokens[i]] = gap;
        else
          range[tokens[i]] = std::min(gap, range[tokens[i]]);
      }
    }
    p = result.q() + 1;
    if (p > result.container_q())
      p = result.container_q();
    q = result.q() + window;
    if (q > result.container_q())
      q = result.container_q();
    if (p <= q) {
      std::string text = warren->txt()->translate(p, q);
      std::vector<std::string> tokens = warren->tokenizer()->split(text);
      for (size_t i = 0; i < tokens.size(); i++) {
        fval gap = result.q() - result.p() + i;
        if (range.find(tokens[i]) == range.end())
          range[tokens[i]] = gap;
        else
          range[tokens[i]] = std::min(gap, range[tokens[i]]);
      }
    }
    for (auto &token : range) {
      if (frequencies.find(token.first) == frequencies.end())
        frequencies[token.first] = 1.0;
      else
        frequencies[token.first]++;
      addr feature = warren->featurizer()->featurize(token.first);
      addr count = warren->idx()->count(feature);
      fval token_prob = (count + 1.0) / (txt_tokens + idx_vocab);
      fval expectation = 1.0 - std::pow(1.0 - token_prob, token.second);
      if (expectations.find(token.first) == expectations.end())
        expectations[token.first] = expectation;
      else
        expectations[token.first] += expectation;
    }
    k++;
    if (k >= depth)
      break;
  }
  std::map<std::string, fval> klds;
  for (auto &&term : frequencies) {
    if (term.second > 1) {
      klds[term.first] =
          frequencies[term.first] *
          std::log(frequencies[term.first] / expectations[term.first]);
      expansion_terms.push_back(term.first);
    }
  }
  std::sort(expansion_terms.begin(), expansion_terms.end(),
            [&](const std::string &a, const std::string &b) -> bool {
              return klds[a] > klds[b];
            });
  if (expansion_terms.size() > expansions)
    expansion_terms.erase(expansion_terms.begin() + expansions,
                          expansion_terms.end());
  if (weighted_query != nullptr) {
    std::map<std::string, fval> expanded_query;
    for (auto &term : expansion_terms)
      expanded_query[term] = klds[term];
    *weighted_query = expanded_query;
  }
  return expansion_terms;
}

// Pseudo-relevance feedback based on KL-Divergence. Designed for use
// with short passages, mostly for question answering purposes.
std::vector<std::string> kld_prf(std::shared_ptr<Warren> warren,
                                 const std::vector<RankingResult> &ranking,
                                 const std::map<std::string, fval> &parameters,
                                 std::map<std::string, fval> *weighted_query) {
  std::vector<std::string> expansion_terms;
  if (ranking.size() == 0)
    return expansion_terms;
  size_t expansions =
      static_cast<size_t>(ranking_parameter("kld", "expansions", parameters));
  size_t depth =
      static_cast<size_t>(ranking_parameter("kld", "depth", parameters));
  addr window =
      static_cast<addr>(ranking_parameter("kld", "window", parameters));
  fval gamma = ranking_parameter("kld", "gamma", parameters);
  std::vector<std::string> texts;
  std::set<cottontail::addr> containers_seen;
  for (auto &&result : ranking) {
    if (containers_seen.find(result.container_p()) != containers_seen.end())
      continue;
    containers_seen.insert(result.container_p());
    addr p, q;
    if (result.container_q() - result.container_p() + 1 <= window) {
      p = result.container_p();
      q = result.container_q();
    } else {
      p = result.p();
      q = result.q();
      if (q - p + 1 < window) {
        addr remaining = window - (q - p + 1);
        p -= remaining / 2;
        q += remaining - (remaining / 2);
        if (p < result.container_p()) {
          q += result.container_p() - p + 1;
          p = result.container_p();
        }
      }
    }
    texts.emplace_back(warren->txt()->translate(p, q));
    if (texts.size() >= depth)
      break;
  }
  if (texts.size() == 0)
    return expansion_terms;
  fval txt_tokens = warren->txt()->tokens();
  fval idx_vocab = warren->idx()->vocab();
  std::map<std::string, fval> frequencies;
  std::map<std::string, fval> expectations;
  for (auto &&text : texts) {
    std::vector<std::string> tokens = warren->tokenizer()->split(text);
    std::set<std::string> seen;
    for (auto &&token : tokens) {
      if (seen.find(token) == seen.end()) {
        seen.insert(token);
        if (frequencies.find(token) == frequencies.end())
          frequencies[token] = 1.0;
        else
          frequencies[token]++;
        addr feature = warren->featurizer()->featurize(token);
        addr count = warren->idx()->count(feature);
        fval token_prob = (count + 1.0) / (txt_tokens + idx_vocab);
        fval expectation = 1.0 - std::pow(1.0 - token_prob, tokens.size());
        if (expectations.find(token) == expectations.end())
          expectations[token] = expectation;
        else
          expectations[token] += expectation;
        ;
      }
    }
  }
  std::map<std::string, fval> tsvs;
  fval max_tsv = 0.0;
  for (auto &&term : frequencies) {
    if (term.second > 1) {
      tsvs[term.first] =
          frequencies[term.first] *
          std::log(frequencies[term.first] / expectations[term.first]);
      expansion_terms.push_back(term.first);
      max_tsv = std::max(max_tsv, tsvs[term.first]);
    }
  }
  std::sort(expansion_terms.begin(), expansion_terms.end(),
            [&](const std::string &a, const std::string &b) -> bool {
              return tsvs[a] > tsvs[b];
            });
  if (expansion_terms.size() > expansions)
    expansion_terms.erase(expansion_terms.begin() + expansions,
                          expansion_terms.end());
  if (weighted_query != nullptr) {
    std::map<std::string, fval> expanded_query;
    for (auto &wt : *weighted_query)
      if (expanded_query.find(wt.first) != expanded_query.end())
        expanded_query[wt.first] += wt.second;
      else
        expanded_query[wt.first] = wt.second;
    for (auto &term : expansion_terms)
      if (expanded_query.find(term) != expanded_query.end())
        expanded_query[term] += gamma * tsvs[term] / max_tsv;
      else
        expanded_query[term] = gamma * tsvs[term] / max_tsv;
    *weighted_query = expanded_query;
  }
  return expansion_terms;
}

// Generate term frequency and document frequency annotations.
// Should be an improvement over tf_idf_annotations
bool tf_df_annotations(std::shared_ptr<Warren> warren, std::string *error) {
  std::string content_key = "container";
  std::string content_query = "";
  if (!warren->get_parameter(content_key, &content_query, error))
    content_query = warren->default_container();
  if (content_query == "") {
    safe_set(error) =
        "tf_df_annotations can't find a definition for item content";
    return false;
  }
  std::unique_ptr<cottontail::Hopper> hopper =
      warren->hopper_from_gcl(content_query, error);
  if (hopper == nullptr)
    return false;
  std::shared_ptr<Featurizer> df_featurizer =
      TaggingFeaturizer::make(warren->featurizer(), "df", error);
  if (df_featurizer == nullptr)
    return false;
  std::shared_ptr<Featurizer> tf_featurizer =
      TaggingFeaturizer::make(warren->featurizer(), "tf", error);
  if (tf_featurizer == nullptr)
    return false;
  std::shared_ptr<Featurizer> total_featurizer =
      TaggingFeaturizer::make(warren->featurizer(), "total", error);
  if (total_featurizer == nullptr)
    return false;
  if (!warren->annotator()->transaction(error))
    return false;
  std::map<addr, addr> df;
  addr HUGE = 1014 * 1024;
  std::vector<addr> ps, qs;
  addr p = minfinity, q, total_items = 0, total_length = 0;
  while (p < maxfinity) {
    hopper->tau(p + 1, &p, &q);
    if (p < maxfinity) {
      total_items++;
      total_length += q - p + 1;
      ps.push_back(p);
      qs.push_back(q);
    }
    if (ps.size() > 0 && (p == maxfinity || qs.back() - ps.front() > HUGE)) {
      std::string text = warren->txt()->translate(ps.front(), qs.back());
      std::vector<std::string> tokens = warren->tokenizer()->split(text);
      for (size_t i = 0; i < ps.size(); i++) {
        std::map<std::string, addr> tf;
        for (addr j = ps[i]; j <= qs[i]; j++) {
          std::string token = tokens[j - ps[0]];
          std::string stem = warren->stemmer()->stem(token);
          if (tf.find(stem) == tf.end())
            tf[stem] = 1;
          else
            tf[stem]++;
        }
        for (auto &token : tf) {
          addr tf_feature = tf_featurizer->featurize(token.first);
          if (!warren->annotator()->annotate(tf_feature, ps[i], ps[i],
                                             token.second, error))
            return false;
          addr df_feature = df_featurizer->featurize(token.first);
          if (df.find(df_feature) == df.end())
            df[df_feature] = 1;
          else
            df[df_feature]++;
        }
      }
      ps.clear();
      qs.clear();
    }
  }
  if (total_items == 0) {
    safe_set(error) = "tf_df_annotations can't find any items for ranking";
    return false;
  }
  for (auto &feature : df)
    if (!warren->annotator()->annotate(feature.first, 0, 0, feature.second,
                                       error))
      return false;
  if (!warren->annotator()->annotate(total_featurizer->featurize("items"), 0, 0,
                                     total_items, error))
    return false;
  if (!warren->annotator()->annotate(total_featurizer->featurize("length"), 0,
                                     0, total_length, error))
    return false;
  if (!warren->annotator()->ready()) {
    warren->annotator()->abort();
    safe_set(error) = "tf_df_annotations can't commit changes";
    return false;
  }
  warren->annotator()->commit();
  std::string stats_key = "statistics";
  std::string stats_name = "df";
  if (!warren->set_parameter(stats_key, stats_name, error))
    return false;
  return true;
}

// Generate annotations for the TF-IDF ranking methods below.
bool tf_idf_annotations(std::shared_ptr<Warren> warren, std::string *error,
                        bool include_unstemmed, bool include_idf,
                        bool include_rsj) {
  if (!(include_idf || include_rsj))
    return true; // weird, but whatever
  if (!warren->annotator()->transaction(error))
    return false;
  std::string working_filename = warren->working()->make_temp("idf");
  std::ofstream anf(working_filename, std::ios::binary);
  if (anf.fail()) {
    safe_set(error) =
        "Cannot create file for tf-idf annotations: " + working_filename;
    return false;
  }
  std::string container = warren->default_container();
  std::unique_ptr<cottontail::Hopper> hopper =
      warren->hopper_from_gcl(container, error);
  if (hopper == nullptr)
    return false;
  addr p, q, N = 0, total_length = 0;
  std::map<std::string, addr> df;
  std::shared_ptr<Featurizer> tf_featurizer =
      TaggingFeaturizer::make(warren->featurizer(), "tf", error);
  if (tf_featurizer == nullptr)
    return false;
  for (hopper->tau(0, &p, &q); p < maxfinity; hopper->tau(p + 1, &p, &q)) {
    N++;
    total_length += q - p + 1;
    std::string text = warren->txt()->translate(p, q);
    std::vector<std::string> tokens = warren->tokenizer()->split(text);
    std::map<std::string, addr> tf;
    if (include_unstemmed) {
      for (auto &token : tokens) {
        if (tf.find(token) == tf.end())
          tf[token] = 1;
        else
          tf[token]++;
        bool stemmed;
        token = warren->stemmer()->stem(token, &stemmed);
        if (stemmed) {
          if (tf.find(token) == tf.end())
            tf[token] = 1;
          else
            tf[token]++;
        }
      }
    } else {
      for (auto &token : tokens) {
        bool stemmed;
        token = warren->stemmer()->stem(token, &stemmed);
        if (tf.find(token) == tf.end())
          tf[token] = 1;
        else
          tf[token]++;
      }
    }
    for (auto &token : tf) {
      if (df.find(token.first) == df.end())
        df[token.first] = 1;
      else
        df[token.first]++;
      Annotation a;
      a.feature = tf_featurizer->featurize(token.first);
      a.p = p;
      a.q = q;
      a.v = 1.0 * token.second;
      anf.write(reinterpret_cast<char *>(&a), sizeof(a));
    }
  }
  std::shared_ptr<Featurizer> idf_featurizer =
      TaggingFeaturizer::make(warren->featurizer(), "idf", error);
  if (idf_featurizer == nullptr)
    return false;
  std::shared_ptr<Featurizer> rsj_featurizer =
      TaggingFeaturizer::make(warren->featurizer(), "rsj", error);
  if (rsj_featurizer == nullptr)
    return false;
  std::shared_ptr<Featurizer> avgl_featurizer =
      TaggingFeaturizer::make(warren->featurizer(), "avgl", error);
  if (avgl_featurizer == nullptr)
    return false;
  Annotation a;
  a.p = a.q = 0; // arbitrarily location for global statistics
  a.feature = avgl_featurizer->featurize("avgl");
  a.v = 1.0 * total_length / N;
  anf.write(reinterpret_cast<char *>(&a), sizeof(a));
  for (auto &token : df) {
    if (include_idf) {
      // inverse document frequency
      fval idf_weight = std::log((1.0 * N) / token.second);
      a.feature = idf_featurizer->featurize(token.first);
      a.v = idf_weight;
      anf.write(reinterpret_cast<char *>(&a), sizeof(a));
    }
    if (include_rsj) {
      // Robertson Sp{\"a}rck Jones weight
      fval rsj_weight =
          std::log((N - token.second + 0.5) / (token.second + 0.5));
      if (rsj_weight > 0.0) {
        a.feature = rsj_featurizer->featurize(token.first);
        a.v = rsj_weight;
        anf.write(reinterpret_cast<char *>(&a), sizeof(a));
      }
    }
  }
  anf.close();
  if (!warren->annotator()->annotate(working_filename, error))
    return false;
  std::remove(working_filename.c_str());
  if (!warren->annotator()->ready()) {
    warren->annotator()->abort();
    safe_set(error) = "tf_idf_annotations can't commit changes";
    return false;
  }
  warren->annotator()->commit();
  std::string key, value;
  key = "unstemmed";
  value = okay(include_unstemmed);
  if (!warren->set_parameter(key, value, error))
    return false;
  key = "idf";
  value = okay(include_idf);
  if (!warren->set_parameter(key, value, error))
    return false;
  key = "rsj";
  value = okay(include_rsj);
  if (!warren->set_parameter(key, value, error))
    return false;
  key = "statistics";
  value = "idf";
  if (!warren->set_parameter(key, value, error))
    return false;
  return true;
}

// Pseudo-relevance feedback based on the description of RM3 in
// Jeffery Dalton, Laura Dietz, James Allan
// Entity Query Feature Expansion using Knowledge Base Links
// SIGIR 2014. See section 2.4
std::vector<std::string> rm3_prf(std::shared_ptr<Warren> warren,
                                 const std::vector<RankingResult> &ranking,
                                 const std::map<std::string, fval> &parameters,
                                 std::map<std::string, fval> *weighted_query) {
  // STUB
  std::vector<std::string> expansion_terms;
  return expansion_terms;
}

// Pseudo-relevance feedback based on Robertson Sp{\"a}rck Jones weights.
// Depends on the existance of these weights in the warren
// See tf_idf_annotations above.
std::vector<std::string> rsj_prf(std::shared_ptr<Warren> warren,
                                 const std::vector<RankingResult> &ranking,
                                 const std::map<std::string, fval> &parameters,
                                 std::map<std::string, fval> *weighted_query) {
  std::vector<std::string> expansion_terms;
  if (ranking.size() == 0)
    return expansion_terms;
  if (!warren->stats()->have("rsj"))
    return expansion_terms;
  size_t expansions =
      static_cast<size_t>(ranking_parameter("rsj", "expansions", parameters));
  size_t depth =
      static_cast<size_t>(ranking_parameter("rsj", "depth", parameters));
  fval gamma = ranking_parameter("rsj", "gamma", parameters);
  std::vector<std::string> texts;
  for (size_t i = 0; i < depth && i < ranking.size(); i++)
    texts.emplace_back(warren->txt()->translate(ranking[i].container_p(),
                                                ranking[i].container_q()));
  if (texts.size() == 0)
    return expansion_terms;
  std::map<std::string, fval> frequencies;
  bool unstemmed = warren->stats()->have("unstemmed");
  for (auto &&text : texts) {
    std::vector<std::string> tokens = warren->tokenizer()->split(text);
    std::set<std::string> seen;
    for (auto &&token : tokens) {
      std::string t;
      if (unstemmed)
        t = token;
      else
        t = warren->stemmer()->stem(token);
      if (seen.find(t) == seen.end()) {
        seen.insert(t);
        if (frequencies.find(t) == frequencies.end())
          frequencies[t] = 1.0;
        else
          frequencies[t]++;
      }
    }
  }
  std::map<std::string, fval> tsvs;
  for (auto &tf : frequencies) {
    fval rsj = warren->stats()->rsj(tf.first);
    if (rsj > 0.0) {
      tsvs[tf.first] = rsj * tf.second;
      expansion_terms.push_back(tf.first);
    }
  }
  std::sort(expansion_terms.begin(), expansion_terms.end(),
            [&](const std::string &a, const std::string &b) -> bool {
              return tsvs[a] > tsvs[b];
            });
  if (expansion_terms.size() > expansions)
    expansion_terms.erase(expansion_terms.begin() + expansions,
                          expansion_terms.end());
  if (weighted_query != nullptr) {
    std::map<std::string, fval> expanded_query;
    for (auto &wt : *weighted_query)
      if (expanded_query.find(wt.first) != expanded_query.end())
        expanded_query[wt.first] += wt.second;
      else
        expanded_query[wt.first] = wt.second;
    for (auto &term : expansion_terms)
      if (expanded_query.find(term) != expanded_query.end())
        expanded_query[term] += gamma;
      else
        expanded_query[term] = gamma;
    *weighted_query = expanded_query;
  }
  return expansion_terms;
}

// Language modeling with Dirichlet smoothing
std::vector<RankingResult>
lmd_ranking(std::shared_ptr<Warren> warren, const std::string &query,
            const std::map<std::string, fval> &parameters) {
  return ranking<lmd_ranking>(warren, query, parameters);
}

std::vector<RankingResult> lmd_ranking(std::shared_ptr<Warren> warren,
                                       const std::string &query) {
  return ranking<lmd_ranking>(warren, query);
}

std::vector<RankingResult>
lmd_ranking(std::shared_ptr<Warren> warren,
            const std::map<std::string, fval> &query,
            const std::map<std::string, fval> &parameters) {
  fval mu = ranking_parameter("lmd", "mu", parameters);
  size_t depth =
      static_cast<size_t>(ranking_parameter("lmd", "depth", parameters));
  std::vector<RankingResult> top;
  if (depth == 0 || query.size() == 0)
    return top;
  struct TermHopper {
    TermHopper(fval qt, fval weight, std::unique_ptr<Hopper> hopper)
        : qt(qt), weight(weight), hopper(std::move(hopper)){};
    fval tf, qt, weight;
    addr p, q;
    std::unique_ptr<Hopper> hopper;
  };
  TaggingFeaturizer tf_featurizer(warren->featurizer(), "tf");
  std::vector<TermHopper> toppers;
  fval tokens = warren->txt()->tokens();
  for (auto &&term : query) {
    addr count =
        warren->idx()->count(warren->featurizer()->featurize(term.first));
    fval weight = tokens / (1.0 * count);
    toppers.emplace_back(
        term.second, weight,
        warren->idx()->hopper(tf_featurizer.featurize(term.first)));
  }
  addr p = maxfinity, q;
  for (auto &topper : toppers) {
    topper.hopper->tau(minfinity + 1, &topper.p, &topper.q, &topper.tf);
    if (topper.p < p) {
      p = topper.p;
      q = topper.q;
    }
  }
  std::vector<RankingResult> current;
  while (p < maxfinity) {
    fval target = (top.size() >= depth ? top.back().score() : 0.0);
    fval length_penalty = std::log(mu + (q - p + 1.0));
    fval score = 0.0;
    addr next_p = maxfinity, next_q = maxfinity;
    for (auto &topper : toppers) {
      if (topper.p == p) {
        fval term_score =
            topper.qt *
            (std::log(mu + topper.tf * topper.weight) - length_penalty);
        score += term_score;
        topper.hopper->tau(p + 1, &topper.p, &topper.q, &topper.tf);
      }
      if (topper.p < next_p) {
        next_p = topper.p;
        next_q = topper.q;
      }
    }
    if (score > target) {
      current.emplace_back(p, q, score);
      if (current.size() == depth) {
        top = top_results(top, current, depth);
        current.clear();
      }
    }
    p = next_p;
    q = next_q;
  }
  top = top_results(top, current, depth);
  return top;
}

// Standard BM25. Index must contain tf_idf annotations (see above)

namespace {

inline fval bm25(fval tf, fval idf, fval l, fval avgl, fval b, fval k1) {
  return tf * idf / (k1 * ((1.0 - b) + b * (l / avgl)) + tf);
}
} // namespace

std::vector<RankingResult>
bm25_ranking(std::shared_ptr<Warren> warren, const std::string &query,
             const std::map<std::string, fval> &parameters) {
  return ranking<bm25_ranking>(warren, query, parameters);
}

std::vector<RankingResult> bm25_ranking(std::shared_ptr<Warren> warren,
                                        const std::string &query) {
  return ranking<bm25_ranking>(warren, query);
}

std::vector<RankingResult>
bm25_ranking(std::shared_ptr<Warren> warren,
             const std::map<std::string, fval> &query,
             const std::map<std::string, fval> &parameters) {
  std::vector<RankingResult> top;
  if (!(warren->stats()->have("avgl") && warren->stats()->have("rsj") &&
        warren->stats()->have("tf")))
    return top;
  size_t depth =
      static_cast<size_t>(ranking_parameter("bm25", "depth", parameters));
  fval b = ranking_parameter("bm25", "b", parameters);
  fval k1 = ranking_parameter("bm25", "k1", parameters);
  if (depth == 0)
    return top;
  if (query.size() == 0)
    return top;
  struct WandHopper {
    WandHopper(fval idf, std::unique_ptr<Hopper> hopper)
        : idf(idf), hopper(std::move(hopper)){};
    fval tf, idf;
    addr p, q;
    std::unique_ptr<Hopper> hopper;
  };
  fval avgl = warren->stats()->avgl();
  std::vector<WandHopper> wand;
  for (auto &wt : query) {
    fval idf = warren->stats()->rsj(wt.first);
    if (idf != 0.0)
      wand.emplace_back(wt.second * idf, warren->stats()->tf_hopper(wt.first));
  }
  for (auto &w : wand)
    w.hopper->tau(minfinity + 1, &w.p, &w.q, &w.tf);
  fval target = 0.0;
  std::vector<RankingResult> current;
  for (;;) {
    target = (top.size() == depth ? top[top.size() - 1].score() : 0.0);
    std::sort(wand.begin(), wand.end(),
              [](const auto &a, const auto &b) -> bool { return a.p < b.p; });
    while (wand.size() > 0 && wand.back().p == maxfinity)
      wand.pop_back();
    fval x = 0.0;
    size_t i;
    for (i = 0; i < wand.size(); i++) {
      x += wand[i].idf;
      if (x > target)
        break;
    }
    if (i == wand.size())
      break;
    addr pivot = wand[i].p;
    addr qivot = wand[i].q;
    fval length = qivot - pivot + 1.0;
    for (size_t j = 0; j < i; j++)
      wand[j].hopper->tau(pivot, &wand[j].p, &wand[j].q, &wand[j].tf);
    fval score = 0.0;
    for (size_t j = 0; j < wand.size(); j++)
      if (wand[j].p == pivot) {
        score += bm25(wand[j].tf, wand[j].idf, length, avgl, b, k1);
        wand[j].hopper->tau(pivot + 1, &wand[j].p, &wand[j].q, &wand[j].tf);
      }
    if (score > target) {
      current.emplace_back(pivot, qivot, score);
      if (current.size() == depth) {
        top = top_results(top, current, depth);
        current.clear();
      }
    }
  }
  top = top_results(top, current, depth);
  return top;
}

std::vector<std::string> qap(std::shared_ptr<Warren> warren,
                             const std::string &query,
                             const std::string &container,
                             const std::map<std::string, fval> &parameters) {
  size_t depth =
      static_cast<size_t>(ranking_parameter("qap", "depth", parameters));
  size_t length =
      static_cast<size_t>(ranking_parameter("qap", "length", parameters));
  size_t passages =
      static_cast<size_t>(ranking_parameter("qap", "passages", parameters));
  size_t window =
      static_cast<size_t>(ranking_parameter("qap", "window", parameters));
  std::vector<RankingResult> covers =
      icover_ranking(warren, query, container, passages);
  fval N = warren->txt()->tokens() + warren->idx()->vocab();
  std::vector<std::string> fragments;
  std::map<std::string, fval> weights;
  std::map<std::string, fval> counts;
  std::set<std::string> seen;
  for (auto &cover : covers) {
    cottontail::addr p = cover.p() - window;
    if (p < cover.container_p())
      p = cover.container_p();
    cottontail::addr q = cover.q() + window;
    if (q > cover.container_q())
      q = cover.container_q();
    std::string fragment = warren->txt()->translate(p, q);
    if (seen.find(fragment) == seen.end()) {
      fragments.emplace_back(fragment);
      std::vector<std::string> tokens = warren->tokenizer()->split(fragment);
      std::map<std::string, addr> locations;
      addr where = p;
      addr location = std::max(cover.p() - where, (addr)0);
      for (auto &token : tokens) {
        if ((locations.find(token) == locations.end()) ||
            (where < locations[token]))
          locations[token] = location;
        where++;
        if (location > 0)
          --location;
        if (where > cover.q())
          location++;
      }
      for (auto &token : locations) {
        if (counts.find(token.first) == counts.end()) {
          addr feature = warren->featurizer()->featurize(token.first);
          fval count = warren->idx()->count(feature) + 1.0;
          counts[token.first] = count;
        }
        fval ft = counts[token.first];
        fval location = token.second;
        fval weight = std::log(N / (ft * (location + 1.0)));
        if (weights.find(token.first) == weights.end())
          weights[token.first] = weight;
        else
          weights[token.first] += weight;
      }
    }
  }
  {
    std::vector<std::string> tokens = warren->tokenizer()->split(query);
    for (auto &token : tokens)
      weights.erase(token);
  }
  for (auto &weight : weights)
    weight.second = weight.second * weight.second * weight.second; // magic cube
  std::vector<std::string> candidates;
  std::vector<size_t> source;
  for (size_t j = 0; j < fragments.size(); j++) {
    std::vector<Token> tokens =
        warren->tokenizer()->tokenize(warren->featurizer(), fragments[j]);
    if (tokens.size() <= length) {
      candidates.emplace_back(fragments[j]);
      source.push_back(j);
    } else {
      for (size_t i = 0; i < tokens.size() - length; i++) {
        candidates.emplace_back(fragments[j].substr(
            tokens[i].offset, tokens[i + length - 1].offset +
                                  tokens[i + length - 1].length -
                                  tokens[i].offset));
        source.push_back(j);
      }
    }
  }
  std::vector<fval> scores;
  std::vector<size_t> indices;
  for (size_t i = 0; i < candidates.size(); i++) {
    std::vector<std::string> tokens = warren->tokenizer()->split(candidates[i]);
    std::set<std::string> seen;
    for (auto &token : tokens)
      if (seen.find(token) == seen.end())
        seen.insert(token);
    fval score = 0.0;
    for (auto &token : seen)
      if (weights.find(token) != weights.end())
        score += weights[token];
    scores.push_back(score);
    indices.push_back(i);
  }
  std::sort(indices.begin(), indices.end(),
            [&](const auto &a, const auto &b) -> bool {
              return scores[a] > scores[b];
            });
  std::vector<std::string> answers;
  {
    std::set<size_t> seen;
    for (size_t i = 0; i < candidates.size() && answers.size() < depth; i++) {
      if (seen.find(source[indices[i]]) == seen.end()) {
        answers.emplace_back(candidates[indices[i]]);
        seen.insert(source[indices[i]]);
      }
    }
  }
  return answers;
}

// Random ranker.
std::vector<RankingResult> random_ranking(std::shared_ptr<Warren> warren,
                                          const std::string &container,
                                          size_t depth) {
  std::vector<RankingResult> top;
  if (depth == 0)
    return top;
  std::string error;
  std::unique_ptr<Hopper> hopper = warren->hopper_from_gcl(container, &error);
  if (hopper == nullptr)
    return top;
  addr p, q;
  srand((int)time(0));
  std::vector<RankingResult> current;
  for (hopper->tau(minfinity + 1, &p, &q); p < maxfinity;
       hopper->tau(p + 1, &p, &q)) {
    fval score = (1.0 * rand()) / (1.0 * rand());
    fval target = (top.size() == depth ? top[top.size() - 1].score() : 0.0);
    if (score > target) {
      current.emplace_back(RankingResult(p, q, score));
      if (current.size() == depth) {
        top = top_results(top, current, depth);
        current.clear();
      }
    }
  }
  top = top_results(top, current, depth);
  return top;
}
} // namespace cottontail
