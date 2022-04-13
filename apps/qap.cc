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

const cottontail::addr WINDOW = 64;

void usage(std::string program_name) {
  std::cerr << "usage: " << program_name << " [--window size] [--score]\n";
}

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  cottontail::addr window = WINDOW;
  bool score = false;
  if (argc == 2 && argv[1] == std::string("--help")) {
    usage(program_name);
    return 0;
  }
  while (argc > 1) {
    if (argc >= 3 &&
        (argv[1] == std::string("--window") || argv[1] == std::string("-w"))) {
      int t = std::stoi(argv[2]);
      if (t < 1)
        t = 1;
      window = t;
      argc -= 2;
      argv += 2;
    } else if (argc >= 2 && (argv[1] == std::string("--score") ||
                             argv[1] == std::string("-s"))) {
      score = true;
      --argc;
      argv++;
    } else {
      usage(program_name);
      return 1;
    }
  }
  std::string error;
  std::string simple = "simple";
  std::string burrow = "/data/hdd3/claclark/c4.burrow";
  std::string veggie6_burrow = "/scratchC/claclark/c4.burrow";
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
  std::mutex labelq_mutex;
  std::mutex output_mutex;
  auto solver = [&]() {
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
      std::vector<cottontail::RankingResult> results =
          icover_ranking(warren, query, container, 1000);
      // tiered_ranking(warren, query, container, 1000);
      output_mutex.lock();
      size_t rank = 1;
      for (auto &result : results) {
        cottontail::addr p = result.p(), q = result.q();
        cottontail::addr cp = result.container_p(), cq = result.container_q();
        if (q - p + 1 < window) {
          cottontail::addr extra = (window - (q - p + 1)) / 2;
          p -= extra;
          if (p < cp)
            p = cp;
        }
        q = p + (window - 1);
        if (q > cq)
          q = cq;
        std::string passage = clean(warren->txt()->translate(p, q));
        if (score)
          std::cout << label << "\t" << rank << "\t" << result.score() << "\t"
                    << passage << "\n";
        else
          std::cout << label << "\t" << rank << "\t" << passage << "\n";
        rank++;
      }
      output_mutex.unlock();
    }
  };
  std::vector<std::thread> workers;
  for (size_t i = 0; i < threads; i++)
    workers.emplace_back(std::thread(solver));
  for (auto &worker : workers)
    worker.join();
  warren->end();
  return 0;
}
