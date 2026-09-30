#include "src/splade.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>

#include "src/hopper.h"
#include "src/json.h"
#include "src/ranking.h"

namespace cottontail {
namespace {

bool better(const RankingResult &a, const RankingResult &b) {
  if (a.score() != b.score())
    return a.score() > b.score();
  return a.p() < b.p();
}

bool rank(std::shared_ptr<Warren> warren,
          const std::map<std::string, fval> &query, const std::string &prefix,
          Hopper *containers, size_t depth, std::vector<RankingResult> *ranking,
          std::string *error) {
  ranking->clear();
  if (depth == 0 || query.empty())
    return true;
  struct Term {
    fval weight;
    std::unique_ptr<Hopper> hopper;
    addr q = maxfinity;
    fval contribution = 0.0;
    Term *next = nullptr;
  };
  std::vector<Term> terms;
  for (const auto &term : query)
    if (term.second > 0.0)
      terms.push_back({term.second, warren->idx()->hopper(
                                       warren->featurizer()->featurize(
                                           prefix + term.first + ":"))});
  // Finish building the vector before threading pointers through its storage.
  Term *head = nullptr;
  auto insert = [&](Term *term) {
    if (term->q == maxfinity)
      return;
    Term **link = &head;
    while (*link != nullptr && (*link)->q < term->q)
      link = &(*link)->next;
    term->next = *link;
    *link = term;
  };
  auto advance = [](Term *term, addr start) {
    addr p;
    fval value;
    term->hopper->tau(start, &p, &term->q, &value);
    if (term->q == maxfinity)
      return;
    // Validate when scoring, so fields outside selected containers are ignored.
    if (!std::isfinite(value) || value < 0.0)
      term->contribution = std::numeric_limits<fval>::quiet_NaN();
    else
      term->contribution = term->weight * value;
  };
  for (auto &term : terms) {
    advance(&term, minfinity + 1);
    insert(&term);
  }
  // The worst retained result is at the top of the heap.
  std::priority_queue<RankingResult, std::vector<RankingResult>,
                      decltype(&better)> top(better);
  while (head != nullptr) {
    addr cp, cq;
    containers->rho(head->q, &cp, &cq);
    if (cp == maxfinity)
      break;
    // Numeric JSON fields occupy single positions. Skip fields in gaps before
    // the next selected container without disturbing hoppers already beyond it.
    if (head->q < cp) {
      Term *term = head;
      head = term->next;
      advance(term, cp);
      insert(term);
      continue;
    }
    fval score = 0.0;
    Term *next_document = head;
    while (next_document != nullptr && next_document->q <= cq) {
      if (std::isnan(next_document->contribution)) {
        safe_error(error) = "Invalid SPLADE document weight";
        return false;
      }
      score += next_document->contribution;
      next_document = next_document->next;
    }
    if (!std::isfinite(score)) {
      safe_error(error) = "SPLADE score overflow";
      return false;
    }
    RankingResult result(cp, cq, score);
    if (score > 0.0) {
      if (top.size() < depth) {
        top.push(result);
      } else if (better(result, top.top())) {
        top.pop();
        top.push(result);
      }
    }
    // Detach this document's prefix before reinserting its advanced hoppers.
    // Containers are disjoint and each vector label occurs once per container.
    Term *term = head;
    head = next_document;
    while (term != next_document) {
      Term *next = term->next;
      advance(term, cq + 1);
      insert(term);
      term = next;
    }
  }
  while (!top.empty()) {
    ranking->push_back(top.top());
    top.pop();
  }
  std::sort(ranking->begin(), ranking->end(), better);
  return true;
}

} // namespace

bool splade(
    std::shared_ptr<Warren> warren,
    const std::map<std::string, std::map<std::string, fval>> &queries,
    const std::map<std::string, std::string> &parameters,
    std::map<std::string, std::vector<std::string>> *results,
    std::string *error, size_t threads, addr *time) {
  if (time != nullptr)
    *time = 0;
  if (results == nullptr || warren == nullptr) {
    safe_error(error) = "SPLADE requires a Warren and results";
    return false;
  }
  results->clear();
  std::string container = ":", id = ":docid:", prefix = ":splade_vector:";
  size_t depth = 1000;
  for (const auto &parameter : parameters) {
    if (parameter.first == "container") {
      container = parameter.second;
    } else if (parameter.first == "id") {
      id = parameter.second;
    } else if (parameter.first == "prefix") {
      prefix = parameter.second;
    } else if (parameter.first == "depth") {
      const std::string &value = parameter.second;
      depth = 0;
      if (value.empty()) {
        safe_error(error) = "Invalid SPLADE depth: " + value;
        return false;
      }
      for (char digit : value) {
        if (digit < '0' || digit > '9' ||
            depth > (std::numeric_limits<size_t>::max() - (digit - '0')) / 10) {
          safe_error(error) = "Invalid SPLADE depth: " + value;
          return false;
        }
        depth = depth * 10 + (digit - '0');
      }
    } else {
      safe_error(error) = "Unknown SPLADE parameter: " + parameter.first;
      return false;
    }
  }
  if (container.empty() || id.empty()) {
    safe_error(error) = "SPLADE container and id queries must not be empty";
    return false;
  }
  for (const auto &query : queries)
    for (const auto &term : query.second)
      if (!std::isfinite(term.second) || term.second < 0.0) {
        safe_error(error) = "Invalid SPLADE query weight for topic: " + query.first;
        return false;
      }
  if (queries.empty())
    return true;

  using Query = std::map<std::string, std::map<std::string, fval>>::value_type;
  std::vector<const Query *> work;
  for (const auto &query : queries)
    work.push_back(&query);
  threads = std::min(allowed_threads(threads), work.size());
  bool end_warren = !warren->started();
  if (end_warren)
    warren->start();
  std::mutex clone_lock, state_lock;
  std::atomic<bool> stop(false);
  std::vector<addr> worker_times(threads, 0);
  std::vector<std::vector<std::string>> output(work.size());
  auto fail = [&](const std::string &local_error) {
    std::lock_guard<std::mutex> _(state_lock);
    if (!stop) {
      safe_set(error) = local_error;
      stop = true;
    }
  };
  auto solver = [&](size_t i) {
    std::string local_error;
    std::shared_ptr<Warren> local;
    {
      std::lock_guard<std::mutex> _(clone_lock);
      if (stop)
        return;
      local = warren->clone(&local_error);
    }
    if (local == nullptr) {
      fail(local_error);
      return;
    }
    std::unique_ptr<Hopper> containers =
        local->hopper_from_gcl(container, &local_error);
    std::unique_ptr<Hopper> ids = local->hopper_from_gcl(id, &local_error);
    if (containers == nullptr || ids == nullptr) {
      local->end();
      fail(local_error);
      return;
    }
    addr start = now();
    for (size_t j = i; j < work.size() && !stop; j += threads) {
      std::vector<RankingResult> ranking;
      if (!rank(local, work[j]->second, prefix, containers.get(), depth,
                &ranking, &local_error)) {
        fail(local_error);
        break;
      }
      for (const auto &result : ranking) {
        addr p, q;
        ids->tau(result.p(), &p, &q);
        if (q <= result.q())
          output[j].push_back(json_translate(local->txt()->translate(p, q)));
      }
    }
    worker_times[i] = now() - start;
    local->end();
  };
  std::vector<std::thread> workers;
  for (size_t i = 0; i < threads; i++)
    workers.emplace_back(solver, i);
  for (auto &worker : workers)
    worker.join();
  if (end_warren)
    warren->end();
  if (stop)
    return false;
  for (size_t j = 0; j < work.size(); j++)
    (*results)[work[j]->first] = std::move(output[j]);
  if (time != nullptr)
    *time = *std::max_element(worker_times.begin(), worker_times.end());
  return true;
}

} // namespace cottontail
