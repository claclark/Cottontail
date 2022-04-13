#include <algorithm>
#include <iomanip>
#include <iostream>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "apps/collection.h"
#include "src/cottontail.h"

const std::string default_burrow = "/data/ssd1/claclark/TREC2020/the.burrow";
const std::string duplicates_filename =
    "/home/claclark/TREC2020/duplicate_list_v1.0.txt";

void dump_weights(const std::string &label,
                  const std::map<std::string, cottontail::fval> &weights) {
  std::cout << "******Terms: " << label << "\n";
  std::vector<std::string> terms;
  for (auto &term : weights)
    terms.push_back(term.first);
  std::sort(terms.begin(), terms.end(),
            [&](const auto &a, const auto &b) -> bool {
              return weights.at(a) > weights.at(b);
            });
  for (auto &term : terms)
    std::cout << term << "(" << weights.at(term) << ")\n";
  std::cout << "------Results: " << label << "\n";
}

std::string strip_newline(const std::string &text) {
  std::string temp = text;
  for (size_t i = 0; i < temp.size(); i++)
    if (temp[i] == '\n')
      temp[i] = ' ';
  return temp;
}

void usage(std::string program_name) {
  std::cerr << "usage: " << program_name
            << " [--burrow burrow] queries [qrels]...\n";
}

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  if (argc == 2 && argv[1] == std::string("--help")) {
    usage(program_name);
    return 0;
  }
  std::string burrow = default_burrow;
  if (argc > 2 &&
      (argv[1] == std::string("-b") || argv[1] == std::string("--burrow"))) {
    burrow = argv[2];
    argc -= 2;
    argv += 2;
  }
  if (argc < 2 || argc > 3) {
    usage(program_name);
    return 1;
  }
  std::string queries_filename = argv[1];
  std::ifstream queriesf(queries_filename);
  if (queriesf.fail()) {
    std::cerr << program_name << ": can't open query file: " + queries_filename;
    return 1;
  }
  std::string qrels_filename = "";
  if (argc == 3)
    qrels_filename = argv[2];
  std::string error;
  std::map<std::string, std::string> canonical;
  if (!cottontail::duplicates_TREC_CAst(duplicates_filename, &canonical,
                                        &error)) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }
  std::string simple = "simple";
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::Warren::make(simple, burrow, &error);
  if (warren == nullptr) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }
  warren->start();
  std::map<std::string, std::map<cottontail::addr, cottontail::fval>> qrels;
  if (qrels_filename != "")
    if (!load_trec_qrels(warren, qrels_filename, &qrels, &error, false)) {
      std::cerr << program_name << ": " << error << "\n";
      return 1;
    }
  std::map<std::string, cottontail::fval> mean_metrics;
  std::string pid = ":pid";
  std::unique_ptr<cottontail::Hopper> pid_hopper =
      warren->hopper_from_gcl(pid, &error);
  if (pid_hopper == nullptr) {
    std::cerr << program_name << ": " << error << "\n";
    return 1;
  }
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
  while (std::getline(queriesf, line)) {
    std::string label = line.substr(0, line.find(" "));
    std::string query = line.substr(line.find(" "));
    if (label == "" || query == "") {
      std::cerr << program_name << ": weird query input: " << line << "\n";
      return 1;
    }
    std::string container = ":paragraph";
    std::map<std::string, cottontail::fval> parameters;
    parameters["depth"] = 32;
    parameters["expansions"] = 32;
    parameters["bm25:depth"] = 32;
    parameters["bm25:b"] = 0.25;
    parameters["bm25:k1"] = 0.5;
    parameters["rsj:expansions"] = 16;
    parameters["rsj:depth"] = 32;
    std::vector<std::string> words = warren->tokenizer()->split(query);
    std::vector<std::string> tokens;
    for (const auto &word : words)
      if (stopwords.find(word) == stopwords.end())
        tokens.push_back(word);
    if (tokens.size() == 0)
      tokens = words;
    std::map<std::string, cottontail::fval> weighted_query;
    for (auto &token : tokens)
      weighted_query[token] = 1.0;
    std::vector<cottontail::RankingResult> ranking =
        cottontail::bm25_ranking(warren, weighted_query, parameters);
    rsj_prf(warren, ranking, parameters, &weighted_query);
    weighted_query.erase("MARCO");
    weighted_query.erase("WAPO");
    weighted_query.erase("CAR");
    if (qrels_filename == "")
      dump_weights(label, weighted_query);
    parameters["depth"] = 2000;
    parameters["bm25:depth"] = 2000;
    std::vector<cottontail::RankingResult> combined_ranking =
        cottontail::bm25_ranking(warren, weighted_query, parameters);
    std::set<std::string> seen;
    size_t rank = 1;
    if (qrels_filename == "") {
      for (auto &result : combined_ranking) {
        cottontail::addr pid_p, pid_q;
        pid_hopper->tau(result.container_p(), &pid_p, &pid_q);
        std::string pid =
            cottontail::trec_docno(warren->txt()->translate(pid_p, pid_q));
        if (canonical.find(pid) != canonical.end())
          pid = canonical[pid];
        if (seen.find(pid) == seen.end()) {
          seen.insert(pid);
          std::cout << label << " Q0 " << pid << " " << rank << " "
                    << result.score() << " claclark\n";
          if (rank <= 1000) {
            std::cout << "++++++Text: " << label << " " << query << "\n";
            std::string contents = warren->txt()->translate(
                result.container_p(), result.container_q());
            std::cout << contents << "\n";
          }
          rank++;
          if (rank > 1000)
            break;
        }
      }
    } else {
      std::map<std::string, cottontail::fval> metrics;
      if (qrels.find(label) != qrels.end()) {
        cottontail::eval(qrels[label], combined_ranking, &metrics);
        cottontail::fval ndcg05 = metrics["ndcg05"];
        char ndcg05_formatted[64];
        sprintf(ndcg05_formatted, "%.4f", ndcg05);
        std::cout << "ndcg_cut_5              " << label << "\t"
                  << ndcg05_formatted << "\n";
        cottontail::mean(metrics, &mean_metrics);
      }
    }
    std::flush(std::cout);
  }
  cottontail::fval mean = 0.0;
  if (mean_metrics.find("ndcg05") != mean_metrics.end())
    mean = mean_metrics["ndcg05"];
  char mean_formatted[64];
  sprintf(mean_formatted, "%.4f", mean);
  std::cout << "ndcg_cut_5              all\t" << mean_formatted << "\n";
  warren->end();
  return 0;
}
