#include <iostream>
#include <map>
#include <mutex>
#include <queue>
#include <regex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "apps/walk.h"
#include "src/cottontail.h"

const std::string DEFAULT_COLLECTION = "/data/hdd3/Collections/trec";

bool load_qrels(
    const std::string &filename,
    std::map<std::string, std::map<std::string, cottontail::fval>> *qrels,
    std::string *error) {
  std::ifstream f(filename);
  if (f.fail()) {
    *error = "Can't open qrels file: " + filename;
    return false;
  }
  std::regex ws_re("\\s+");
  std::string line;
  cottontail::fval max_level = 0.0;
  while (std::getline(f, line)) {
    std::vector<std::string> field{
        std::sregex_token_iterator(line.begin(), line.end(), ws_re, -1), {}};
    if (field.size() != 4) {
      *error = "Format error in qrels file: " + filename;
      return false;
    }
    cottontail::fval level = std::max(0.0, atof(field[3].c_str()));
    if (level > 0.0) {
      max_level = std::max(level, max_level);
      (*qrels)[field[0]][field[2]] = level;
    }
  }
  for (auto &&topic : (*qrels))
    for (auto &&docno : topic.second)
      (*qrels)[topic.first][docno.first] /= max_level;
  return true;
}

bool load_all_qrels(
    const std::string &qrel_directory,
    std::map<std::string, std::map<std::string, cottontail::fval>> *qrels,
    std::string *error) {
  if (load_qrels(qrel_directory + "/trec4.qrels", qrels, error) &&
      load_qrels(qrel_directory + "/trec5.qrels", qrels, error) &&
      load_qrels(qrel_directory + "/trec6.qrels", qrels, error) &&
      load_qrels(qrel_directory + "/trec7.qrels", qrels, error))
    return true;
  else
    return false;
}

bool load_queries(const std::string &filename,
                  std::map<std::string, std::string> *queries,
                  std::string *error) {
  std::ifstream f(filename);
  if (f.fail()) {
    *error = "Can't open queries file: " + filename;
    return false;
  }
  std::string line;
  while (std::getline(f, line)) {
    std::string separator = " ";
    if (line.find(separator) == std::string::npos)
      separator = " ";
    if (line.find(separator) == std::string::npos) {
      *error = "Weird query input: " + line;
      return false;
    }
    std::string topic = line.substr(0, line.find(separator));
    std::string query = line.substr(line.find(separator));
    if (topic == "" || query == "") {
      *error = "Weird query input: " + line;
      return false;
    }
    (*queries)[topic] = query;
  }
  return true;
}

std::map<std::string, std::map<std::string, cottontail::fval>> invert_qrels(
    std::map<std::string, std::map<std::string, cottontail::fval>> qrels) {
  std::map<std::string, std::map<std::string, cottontail::fval>> slerq;
  for (auto &&topic : qrels)
    for (auto &&qrel : topic.second)
      slerq[qrel.first][topic.first] = qrel.second;
  return slerq;
}

std::shared_ptr<cottontail::Warren> make_warren(std::string *error) {
  std::string burrow = "trec.burrow";
  std::shared_ptr<cottontail::Working> working =
      cottontail::Working::mkdir(burrow, error);
  if (working == nullptr)
    return nullptr;
  std::shared_ptr<cottontail::Featurizer> featurizer =
      cottontail::Featurizer::make("hashing", "", error);
  if (featurizer == nullptr)
    return nullptr;
  std::shared_ptr<cottontail::Tokenizer> tokenizer =
      cottontail::Tokenizer::make("ascii", "xml", error);
  if (tokenizer == nullptr)
    return nullptr;
  std::shared_ptr<cottontail::Bigwig> bigwig =
      cottontail::Bigwig::make(working, featurizer, tokenizer, error);
  if (bigwig == nullptr)
    return nullptr;
  bigwig->start();
  std::string stemmer_name = "porter";
  std::string stemmer_recipe = "";
  std::shared_ptr<cottontail::Stemmer> stemmer =
      cottontail::Stemmer::make(stemmer_name, stemmer_recipe, error);
  if (stemmer == nullptr)
    return nullptr;
  if (!bigwig->set_stemmer(stemmer, error))
    return nullptr;
  std::string container_query = "(... <DOC> </DOC>)";
  if (!bigwig->set_default_container(container_query, error))
    return nullptr;
  std::string id_key = "id";
  std::string id_query = "(... <DOCNO> </DOCNO>)";
  if (!bigwig->set_parameter(id_key, id_query, error))
    return nullptr;
  bigwig->end();
  return bigwig;
}

void usage(std::string program_name) {
  std::cerr << "usage: " << program_name << " collection queries qrels\n";
}

bool walk_directory(const std::string &directory,
                    std::vector<std::string> *files, std::string *error) {
  char *dir = strdup(directory.c_str());
  if (dir == nullptr) {
    *error = "Out of memory";
    return false;
  }
  if (!cottontail::walk_filesystem(dir, files)) {
    *error = "Can't walk: " + directory;
    free(dir);
    return false;
  }
  free(dir);
  return true;
}

bool walk_disk2(const std::string &trec_directory,
                std::vector<std::string> *files, std::string *error) {
  return walk_directory(trec_directory + "/tipster_disk_2", files, error);
}

bool walk_disk3(const std::string &trec_directory,
                std::vector<std::string> *files, std::string *error) {
  return walk_directory(trec_directory + "/tipster_disk_3", files, error);
}

bool walk_disk4(const std::string &trec_directory,
                std::vector<std::string> *files, std::string *error) {
  return walk_directory(trec_directory + "/trec_disk_4", files, error);
}

bool walk_disk5(const std::string &trec_directory,
                std::vector<std::string> *files, std::string *error) {
  return walk_directory(trec_directory + "/trec_disk_5", files, error);
}

bool walk_cr(const std::string &trec_directory, std::vector<std::string> *files,
             std::string *error) {
  return walk_directory(trec_directory + "/trec_disk_4/cr", files, error);
}

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  if (argc == 2 && argv[1] == std::string("--help")) {
    usage(program_name);
    return 0;
  }
  std::string trec_directory;
  std::string query_directory;
  std::string qrel_directory;
  if (argc != 1 && argc != 4) {
    usage(program_name);
    return 1;
  }
  if (argc == 4) {
    trec_directory = argv[1];
    query_directory = argv[2];
    qrel_directory = argv[3];
  } else {
    trec_directory = DEFAULT_COLLECTION;
    query_directory = qrel_directory = DEFAULT_COLLECTION + "/Q";
  }
  std::string error;
  std::map<std::string, std::string> trec4_queries;
  std::map<std::string, std::string> trec5_queries;
  std::map<std::string, std::string> trec6_queries;
  std::map<std::string, std::string> trec7_queries;
  if (!load_queries(query_directory + "/trec4.queries", &trec4_queries,
                    &error) ||
      !load_queries(query_directory + "/trec5.queries", &trec5_queries,
                    &error) ||
      !load_queries(query_directory + "/trec6.queries", &trec6_queries,
                    &error) ||
      !load_queries(query_directory + "/trec7.queries", &trec7_queries,
                    &error)) {
    std::cerr << program_name << ": " << error << "\n" << std::flush;
    exit(1);
  }
  std::vector<std::string> disk2_files;
  std::vector<std::string> disk3_files;
  std::vector<std::string> disk4_files;
  std::vector<std::string> disk5_files;
  std::vector<std::string> cr_files;
  if (!walk_disk2(trec_directory, &disk2_files, &error) ||
      !walk_disk3(trec_directory, &disk3_files, &error) ||
      !walk_disk4(trec_directory, &disk4_files, &error) ||
      !walk_disk5(trec_directory, &disk5_files, &error) ||
      !walk_cr(trec_directory, &cr_files, &error)) {
    std::cerr << program_name << ": " << error << "\n" << std::flush;
    exit(1);
  }
  std::map<std::string, std::map<std::string, cottontail::fval>> qrels;
  if (!load_all_qrels(qrel_directory, &qrels, &error)) {
    std::cerr << program_name << ": " << error << "\n" << std::flush;
    exit(1);
  }
  std::map<std::string, std::map<std::string, cottontail::fval>> slerq =
      invert_qrels(qrels);
  std::shared_ptr<cottontail::Warren> trec_warren = make_warren(&error);
  if (trec_warren == nullptr) {
    std::cerr << program_name << ": " << error << "\n" << std::flush;
    exit(1);
  }
  std::string container_query;
  if (!trec_warren->get_parameter("container", &container_query, &error)) {
    std::cerr << program_name << ": " << error << "\n" << std::flush;
    exit(1);
  }
  std::string id_query;
  if (!trec_warren->get_parameter("id", &id_query, &error)) {
    std::cerr << program_name << ": " << error << "\n" << std::flush;
    exit(1);
  }
  bool verbose = false;
  std::mutex output_mutex;

  // Scriber worker
  std::queue<std::string> docq;
  std::mutex docq_mutex;
  auto scribe_worker = [&]() {
    std::string error;
    std::shared_ptr<cottontail::Warren> warren = trec_warren->clone(&error);
    if (warren == nullptr) {
      output_mutex.lock();
      std::cerr << program_name << ": " << error << "\n" << std::flush;
      output_mutex.unlock();
      exit(1);
    }
    for (;;) {
      std::vector<std::string> doc;
      bool done = false;
      docq_mutex.lock();
      if (docq.empty()) {
        done = true;
      } else {
        doc.push_back(docq.front());
        docq.pop();
      }
      docq_mutex.unlock();
      if (done)
        return;
      if (verbose) {
        output_mutex.lock();
        std::cerr << "Scribing: " << doc[0] << "\n" << std::flush;
        output_mutex.unlock();
      }
      if (!cottontail::scribe_files(doc, cottontail::Scribe::make(warren),
                                    &error)) {
        output_mutex.lock();
        std::cerr << program_name << ": " << error << "\n" << std::flush;
        output_mutex.unlock();
        exit(1);
      }
      if (verbose) {
        output_mutex.lock();
        std::cerr << "Annotating: " << doc[0] << "\n" << std::flush;
        output_mutex.unlock();
      }
      std::string query = "(>> file: (<< (>> filename: \"" + doc[0] + "\") \"" +
                          doc[0] + "\"))";
      warren->start();
      std::unique_ptr<cottontail::Hopper> hopper =
          warren->hopper_from_gcl(query, &error);
      if (hopper == nullptr) {
        output_mutex.lock();
        std::cerr << program_name << ": " << error << "\n" << std::flush;
        output_mutex.unlock();
        exit(1);
      }
      cottontail::addr p, q;
      hopper->tau(cottontail::minfinity + 1, &p, &q);
      if (p == cottontail::maxfinity) {
        output_mutex.lock();
        std::cerr << program_name
                  << ": Cannot add tf annotations for: " + doc[0] << "\n"
                  << std::flush;
        output_mutex.unlock();
        exit(1);
      }
      cottontail::tf_annotations(warren, nullptr, p, q);
      std::unique_ptr<cottontail::Hopper> doc_hopper =
          warren->hopper_from_gcl(container_query);
      if (doc_hopper == nullptr) {
        output_mutex.lock();
        std::cerr << program_name << ": Bad container query\n";
        output_mutex.unlock();
        exit(1);
      }
      std::unique_ptr<cottontail::Hopper> id_hopper =
          warren->hopper_from_gcl(id_query);
      if (id_hopper == nullptr) {
        output_mutex.lock();
        std::cerr << program_name << ": Bad identifer query\n";
        output_mutex.unlock();
        exit(1);
      }
      bool in_transaction = false;
      cottontail::addr p0, q0;
      for (id_hopper->tau(p, &p0, &q0); q0 < q;
           id_hopper->tau(p0 + 1, &p0, &q0)) {
        std::string docno =
            cottontail::trec_docno(warren->txt()->translate(p0, q0));
        cottontail::addr p1, q1;
        doc_hopper->rho(p0, &p1, &q1);
        auto where = slerq.find(docno);
        if (where != slerq.end()) {
          if (!in_transaction) {
            if (!warren->transaction(&error)) {
              output_mutex.lock();
              std::cerr << program_name << ": " << error << "\n" << std::flush;
              output_mutex.unlock();
              exit(1);
            }
            in_transaction = true;
          }
          for (auto &&qrel : where->second) {
            std::string label = "qrel:" + qrel.first;
            warren->annotator()->annotate(
                warren->featurizer()->featurize(label), p1, q0, qrel.second);
          }
        }
      }
      if (in_transaction) {
        if (!warren->ready(&error)) {
          output_mutex.lock();
          std::cerr << program_name << ": " << error << "\n" << std::flush;
          output_mutex.unlock();
          exit(1);
        }
        warren->commit();
      }
      warren->end();
      if (verbose) {
        output_mutex.lock();
        std::cerr << "Done: " << doc[0] << "\n" << std::flush;
        output_mutex.unlock();
      }
    }
  };

  // Ranking worker
  std::string pipeline =
      "bm25:b=0.298514 bm25:k1=0.786383 stop stem bm25 rsj:depth=19.2284 "
      "rsj:expansions=20.8663 rsj:gamma=0.186011 rsj bm25:b=0.362828 "
      "bm25:k1=0.711716 stop stem bm25";

  bool stop = false;
  auto ranking_worker = [&](int trec, std::string topic, std::string query) {
    std::string error;
    std::shared_ptr<cottontail::Warren> warren = trec_warren->clone(&error);
    if (warren == nullptr) {
      output_mutex.lock();
      std::cerr << program_name << ": " << error << "\n" << std::flush;
      output_mutex.unlock();
      exit(1);
    }
    bool done = false;
    while (!done) {
      done = stop;
      warren->start();
      std::shared_ptr<cottontail::Ranker> rank =
          cottontail::Ranker::from_pipeline(pipeline, warren, &error);
      if (rank == nullptr) {
        output_mutex.lock();
        std::cerr << program_name << ": " << error << "\n" << std::flush;
        output_mutex.unlock();
        exit(1);
      }
      std::vector<cottontail::RankingResult> ranking = (*rank)(query);
      std::map<cottontail::addr, cottontail::fval> lrels;
      std::string label = "qrel:" + topic;
      std::unique_ptr<cottontail::Hopper> hopper =
          warren->hopper_from_gcl(label, &error);
      if (hopper == nullptr) {
        output_mutex.lock();
        std::cerr << program_name << ": " << error << "\n" << std::flush;
        output_mutex.unlock();
        exit(1);
      }
      cottontail::addr p, q;
      cottontail::fval v;
      for (hopper->tau(cottontail::minfinity + 1, &p, &q, &v);
           p < cottontail::maxfinity; hopper->tau(p + 1, &p, &q, &v))
        lrels[p] = v;
      warren->end();
      cottontail::addr fake = cottontail::maxfinity / 2;
      while (lrels.size() < qrels[topic].size())
        lrels[fake++] = 1;
      std::map<std::string, cottontail::fval> metrics;
      cottontail::eval(lrels, ranking, &metrics);
      output_mutex.lock();
      std::cout << time(NULL) << " " << trec << " " << topic << " "
                << metrics["ap"] << "\n"
                << std::flush;
      output_mutex.unlock();
      sleep(2);
    }
  };

  std::queue<std::string> delq;
  std::mutex delq_mutex;
  auto erase_worker = [&]() {
    std::string error;
    std::shared_ptr<cottontail::Warren> warren = trec_warren->clone(&error);
    if (warren == nullptr) {
      output_mutex.lock();
      std::cerr << program_name << ": " << error << "\n" << std::flush;
      output_mutex.unlock();
      exit(1);
    }
    for (;;) {
      std::string filename;
      bool done = false;
      delq_mutex.lock();
      if (delq.empty()) {
        done = true;
      } else {
        filename = delq.front();
        delq.pop();
      }
      delq_mutex.unlock();
      if (done)
        return;
      std::string query = "(>> file: (>> filename: \"" + filename + "\"))";
      warren->start();
      std::unique_ptr<cottontail::Hopper> hopper =
          warren->hopper_from_gcl(query, &error);
      if (hopper == nullptr) {
        output_mutex.lock();
        std::cerr << program_name << ": " << error << "\n" << std::flush;
        output_mutex.unlock();
        exit(1);
      }
      cottontail::addr p, q;
      hopper->tau(0, &p, &q);
      if (p < cottontail::maxfinity) {
        if (!warren->transaction(&error) ||
            !warren->annotator()->erase(p, q, &error) ||
            !warren->ready(&error)) {
          output_mutex.lock();
          std::cerr << program_name << ": " << error << "\n" << std::flush;
          output_mutex.unlock();
          exit(1);
        }
        warren->commit();
      }
      warren->end();
    }
  };

  std::vector<std::thread> rankers;
  for (auto &&topic : trec4_queries)
    rankers.emplace_back(
        std::thread(ranking_worker, 4, topic.first, topic.second));
  for (auto &&topic : trec5_queries)
    rankers.emplace_back(
        std::thread(ranking_worker, 5, topic.first, topic.second));
  for (auto &&topic : trec6_queries)
    rankers.emplace_back(
        std::thread(ranking_worker, 6, topic.first, topic.second));

  size_t threads =
      std::max(std::thread::hardware_concurrency(), (unsigned int)2);

  // TREC-4
  {
    for (auto &&f : disk2_files)
      docq.push(f);
    for (auto &&f : disk3_files)
      docq.push(f);
    std::vector<std::thread> scribers;
    for (size_t i = 0; i < threads; i++)
      scribers.emplace_back(std::thread(scribe_worker));
    for (auto &scriber : scribers)
      scriber.join();
    sleep(30);
  }

  // TREC-5
  {
    for (auto &&f : disk3_files)
      delq.push(f);
    std::thread eraser(erase_worker);
    for (auto &&f : disk4_files)
      docq.push(f);
    std::vector<std::thread> scribers;
    for (size_t i = 0; i < threads; i++)
      scribers.emplace_back(std::thread(scribe_worker));
    for (auto &scriber : scribers)
      scriber.join();
    sleep(30);
    eraser.join();
  }

  // TREC-6
  {
    for (auto &&f : disk2_files)
      delq.push(f);
    std::thread eraser(erase_worker);
    for (auto &&f : disk5_files)
      docq.push(f);
    std::vector<std::thread> scribers;
    for (size_t i = 0; i < threads; i++)
      scribers.emplace_back(std::thread(scribe_worker));
    for (auto &scriber : scribers)
      scriber.join();
    sleep(30);
    eraser.join();
  }

  stop = true;
  for (auto &ranker : rankers)
    ranker.join();

  return 0;
}
