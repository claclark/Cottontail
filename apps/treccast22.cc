#include <algorithm>
#include <iostream>
#include <map>
#include <mutex>
#include <queue>
#include <regex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "src/cottontail.h"

void usage(std::string program_name) {
  std::cerr << "usage: " << program_name << " [--passages]\n";
}

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  std::string error;
  std::string simple = "simple";
  std::string c4_burrow = "/data/hdd3/claclark/c4.burrow";
  std::string veggie6_c4_burrow = "/scratchC/claclark/c4.burrow";
  std::string cast_burrow = "/data/hdd3/claclark/cast22.burrow";
  std::string veggie6_cast_burrow = "/scratchC/claclark/cast22.burrow";
  bool print_passages = false;
  if (argc > 1 &&
      (argv[1] == std::string("-p") || argv[1] == std::string("--passages"))) {
    print_passages = true;
    --argc;
    argv++;
  }
  std::shared_ptr<cottontail::Warren> c4_warren =
      cottontail::Warren::make(simple, c4_burrow, &error);
  if (c4_warren == nullptr) {
    c4_warren = cottontail::Warren::make(simple, veggie6_c4_burrow, &error);
    if (c4_warren == nullptr) {
      std::cerr << program_name << ": " << error << "\n";
      return 1;
    }
  }
  std::shared_ptr<cottontail::Warren> cast_warren =
      cottontail::Warren::make(simple, cast_burrow, &error);
  if (cast_warren == nullptr) {
    cast_warren = cottontail::Warren::make(simple, veggie6_cast_burrow, &error);
    if (cast_warren == nullptr) {
      std::cerr << program_name << ": " << error << "\n";
      return 1;
    }
  }
  c4_warren->start();
  cast_warren->start();
  size_t threads = 2 * std::thread::hardware_concurrency();
  std::string container = ":text";
  std::vector<std::string> stopword_list = {
      "a",           "about", "an",    "and",  "any",    "are",     "as",
      "at",          "be",    "being", "by",   "can",    "defines", "describe",
      "description", "did",   "do",    "does", "for",    "from",    "give",
      "had",         "has",   "have",  "his",  "how",    "i",       "if",
      "in",          "is",    "isn",   "it",   "its",    "like",    "many",
      "may",         "me",    "much",  "my",   "of",     "on",      "once",
      "one",         "ones",  "or",    "s",    "should", "so",      "some",
      "such",        "t",     "tell",  "than", "that",   "the",     "their",
      "them",        "there", "these", "they", "this",   "to",      "use",
      "using",       "was",   "we",    "well", "were",   "what",    "when",
      "where",       "which", "who",   "why",  "will",   "with",    "you",
      "your",
  };
  std::set<std::string> stopwords(stopword_list.begin(), stopword_list.end());
  std::string line;
  std::map<std::string, std::string> questions;
  std::map<std::string, std::string> queries;
  std::queue<std::string> labelq;
  std::mutex labelq_mutex;
  while (std::getline(std::cin, line)) {
    std::string separator = "\t";
    if (line.find(separator) == std::string::npos)
      separator = " ";
    if (line.find(separator) == std::string::npos) {
      std::cerr << program_name << ": weird question: " << line;
      return 1;
    }
    std::string label = line.substr(0, line.find(separator));
    std::string question = line.substr(line.find(separator));
    if (label == "" || question == "") {
      std::cerr << program_name << ": weird question: " << line;
      return 1;
    }
    std::vector<std::string> words = cast_warren->tokenizer()->split(question);
    std::vector<std::string> keywords;
    std::set<std::string> seen;
    std::string query;
    for (const auto &word : words)
      if (stopwords.find(word) == stopwords.end()) {
        keywords.push_back(word);
        seen.insert(word);
      }
    if (keywords.size() > 1) {
      query = keywords[0];
      for (size_t i = 0; i < keywords.size(); i++)
        query += (" " + keywords[i]);
    } else {
      query = question;
    }
    questions[label] = question;
    queries[label] = query;
    labelq.push(label);
  }
  auto clean = [](std::string s) {
    for (size_t i = 0; i < s.length(); i++)
      if (s[i] == '\n' || s[i] == '\t')
        s[i] = ' ';
    return s;
  };
  std::string pid = ":pid";
  std::string pipeline =
      "stem bm25:b=0.447428 bm25:k1=1.18281 bm25 rsj:depth=17.211 "
      "rsj:expansions=25.6182 rsj:gamma=0.145504 rsj stem bm25:b=0.481409 "
      "bm25:k1=1.02941 bm25";
  std::shared_ptr<cottontail::Ranker> rank =
      cottontail::Ranker::from_pipeline(pipeline, cast_warren, &error);
  std::mutex output_mutex;
  auto solver = [&](size_t i) {
    std::unique_ptr<cottontail::Hopper> pid_hopper =
        cast_warren->hopper_from_gcl(pid);
    assert(pid_hopper != nullptr);
    for (;;) {
      std::string label;
      std::string query;
      std::string question;
      bool done = false;
      labelq_mutex.lock();
      if (labelq.empty()) {
        done = true;
      } else {
        label = labelq.front();
        labelq.pop();
        query = queries[label];
        question = questions[label];
      }
      labelq_mutex.unlock();
      if (done)
        return;
      std::vector<cottontail::RankingResult> results;
      std::vector<std::vector<cottontail::RankingResult>> rankings;
      rankings.emplace_back((*rank)(query));
      std::vector<cottontail::RankingResult> c4_icovers =
          icover_ranking(c4_warren, query, container, 32);
      std::map<std::string, cottontail::fval> parameters;
      parameters["kld:expansions"] = 16;
      parameters["kld:depth"] = 32;
      parameters["bm25:b"] = 0.447428;
      parameters["bm25:k1"] = 1.18281;
      std::map<std::string, cottontail::fval> weighted_query;
      std::vector<std::string> terms =
          qa_prf(c4_warren, c4_icovers, parameters, &weighted_query);
      std::map<std::string, cottontail::fval> stemmed_weighted_query;
      for (auto &term : weighted_query)
        stemmed_weighted_query[cast_warren->stemmer()->stem(term.first)] = 0.0;
      for (auto &term : weighted_query)
        stemmed_weighted_query[cast_warren->stemmer()->stem(term.first)] +=
            term.second;
      rankings.emplace_back(
          bm25_ranking(cast_warren, stemmed_weighted_query, parameters));
      results = rrf_fusion(rankings);
      output_mutex.lock();
      size_t rank = 1;
      for (auto &result : results) {
        cottontail::addr pid_p, pid_q;
        pid_hopper->tau(result.container_p(), &pid_p, &pid_q);
        std::string pid =
            cottontail::trec_docno(cast_warren->txt()->translate(pid_p, pid_q));
        std::cout << label << " Q0 " << pid << " " << rank << " "
                  << result.score() << " claclark";
        if (print_passages) {
          std::string passage = clean(
              cast_warren->txt()->translate(pid_q + 1, result.container_q()));
          std::cout << "\t" << passage << "\n";
        } else {
          std::cout << "\n";
        }
        rank++;
      }
      std::flush(std::cout);
      output_mutex.unlock();
    }
  };
  std::vector<std::thread> workers;
  for (size_t i = 0; i < threads; i++)
    workers.emplace_back(std::thread(solver, i));
  for (auto &worker : workers)
    worker.join();
  cast_warren->end();
  c4_warren->end();
  return 0;
}
