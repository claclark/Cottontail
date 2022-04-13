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
  std::cerr << "usage: " << program_name << " [--stopwords]\n";
}

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  std::string error;
  std::string simple = "simple";
  std::string burrow = "/data/hdd3/claclark/TREC-DL-2021/the.burrow";
  std::string veggie6_burrow = "/scratchC/claclark/trecdl21.burrow";
  bool remove_stopwords = false;
  if (argc > 1) {
    if (argv[1] == std::string("--help")) {
        usage(program_name);
        return 0;
      }
    else if (argc == 2 && argv[1] == std::string("--stopwords")) {
      remove_stopwords = true;
    } else {
      usage(program_name);
      return 1;
    }
  }
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::Warren::make(simple, burrow, &error);
  if (warren == nullptr) {
    warren = cottontail::Warren::make(simple, veggie6_burrow, &error);
    if (warren == nullptr) {
      std::cerr << program_name << ": " << error << "\n";
      return 1;
    }
  }
  warren->start();
  size_t threads = 2 * std::thread::hardware_concurrency();
  std::string container = ":paragraph";
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
  std::mutex labelq_mutex;
  std::queue<std::string> labelq;
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
    std::vector<std::string> words = warren->tokenizer()->split(question);
    std::vector<std::string> keywords;
    std::set<std::string> seen;
    std::string query;
    for (const auto &word : words)
      if (!remove_stopwords || stopwords.find(word) == stopwords.end()) {
        keywords.push_back(word);
        seen.insert(word);
      }
    if (keywords.size() > 0) {
      query = keywords[0];
      for (size_t i = 1; i < keywords.size(); i++)
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
  std::string pipeline = "bm25:b=0.25 bm25:k1=0.5 bm25";
  std::shared_ptr<cottontail::Ranker> rank =
      cottontail::Ranker::from_pipeline(pipeline, warren, &error);
  if (rank == nullptr) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }
  std::mutex output_mutex;
  auto solver = [&](size_t i) {
    std::unique_ptr<cottontail::Hopper> pid_hopper =
        warren->hopper_from_gcl(pid);
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
      results = (*rank)(query);
      output_mutex.lock();
      int i = 1;
      for (auto &result : results) {
        cottontail::addr pid_p, pid_q;
        pid_hopper->tau(result.container_p(), &pid_p, &pid_q);
        std::string pid =
            cottontail::trec_docno(warren->txt()->translate(pid_p, pid_q));
        std::cout << label << "\t" << pid << "\t" << i << "\t" << result.score()
                  << "\t" << question << "\t";
        std::string passage =
            clean(warren->txt()->translate(pid_q + 1, result.container_q()));
        std::cout << "\t" << passage << "\n";
        i++;
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
  warren->end();
  return 0;
}
