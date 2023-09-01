#include <cassert>
#include <iostream>
#include <map>
#include <mutex>
#include <queue>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "apps/walk.h"
#include "src/cottontail.h"

#define ASSERT_EQ(a, b) (assert((a) == (b)))
#define ASSERT_NE(a, b) (assert((a) != (b)))
#define ASSERT_TRUE(a) (assert((a)))

void usage(std::string program_name) {
  std::cerr << "usage: " << program_name
            << " [--threads n] queries quels documents...\n";
}

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  std::string burrow;
  if (argc == 2 && argv[1] == std::string("--help")) {
    usage(program_name);
    return 0;
  }
  size_t threads = 2 * (std::thread::hardware_concurrency() + 1);
  if (argc > 2 &&
      (argv[1] == std::string("-t") || argv[1] == std::string("--threads"))) {
    try {
      int t = std::stoi(argv[2]);
      if (t > 0)
        threads = t;
    } catch (std::exception &e) {
    }
    argc -= 2;
    argv += 2;
  }
  if (argc < 4) {
    usage(program_name);
    return 1;
  }
  std::string queries_filename = argv[1];
  std::string qrels_filename = argv[2];
  std::vector<std::string> documents;
  for (int i = 3; i < argc; i++)
    if (!cottontail::walk_filesystem(argv[i], &documents)) {
      std::cerr << program_name << ": can't walk \"" << argv[i] << "\"\n";
      return -1;
    }
  std::queue<std::string> docq;
  for (auto &document : documents)
    docq.push(document);

  std::shared_ptr<cottontail::Featurizer> featurizer =
      cottontail::Featurizer::make("hashing", "");
  ASSERT_NE(featurizer, nullptr);
  std::shared_ptr<cottontail::Tokenizer> tokenizer =
      cottontail::Tokenizer::make("ascii", "xml");
  ASSERT_NE(tokenizer, nullptr);
  std::shared_ptr<cottontail::Bigwig> bigwig =
      cottontail::Bigwig::make(nullptr, featurizer, tokenizer);
  ASSERT_NE(bigwig, nullptr);

  bigwig->start();
  std::string stemmer_name = "porter";
  std::string stemmer_recipe = "";
  std::shared_ptr<cottontail::Stemmer> stemmer =
      cottontail::Stemmer::make(stemmer_name, stemmer_recipe);
  ASSERT_NE(stemmer, nullptr);
  ASSERT_TRUE(bigwig->set_stemmer(stemmer));
  std::string container_query = "(... <DOC> </DOC>)";
  ASSERT_TRUE(bigwig->set_default_container(container_query));
  std::string id_key = "id";
  std::string id_query = "(... <DOCNO> </DOCNO>)";
  ASSERT_TRUE(bigwig->set_parameter(id_key, id_query));
  bigwig->end();

  bool stop = false;
  std::mutex docq_mutex;
  std::mutex output_mutex;
  auto scribe_worker = [&]() {
    bool verbose = false;
    std::shared_ptr<cottontail::Warren> warren = bigwig->clone();
    ASSERT_NE(warren, nullptr);
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
        std::cout << "Scribing: " << doc[0] << "\n";
        output_mutex.unlock();
      }
      ASSERT_TRUE(
          cottontail::scribe_files(doc, cottontail::Scribe::make(warren)));
      if (verbose) {
        output_mutex.lock();
        std::cout << "Annotating: " << doc[0] << "\n";
        output_mutex.unlock();
      }
      std::string query = "(>> file: (<< (>> filename: \"" + doc[0] + "\") \"" +
                          doc[0] + "\"))";
      std::shared_ptr<cottontail::Warren> BAD = bigwig->clone();
      warren->start();
      std::unique_ptr<cottontail::Hopper> hopper =
          warren->hopper_from_gcl(query);
      ASSERT_NE(hopper, nullptr);
      cottontail::addr p, q;
      hopper->tau(cottontail::minfinity + 1, &p, &q);
      ASSERT_NE(p, cottontail::maxfinity);
      BAD->start();
      cottontail::tf_annotations(BAD, nullptr, p, q);
      BAD->end();
      warren->end();
      if (verbose) {
        output_mutex.lock();
        std::cout << "Done: " << doc[0] << "\n";
        output_mutex.unlock();
      }
    }
  };
  auto search_worker = [&](std::string query) {
    std::shared_ptr<cottontail::Warren> warren = bigwig->clone();
    output_mutex.lock();
    std::cout << "Seaching " << query << "\n" << std::flush;
    output_mutex.unlock();
    bool done = false;
    cottontail::addr last = cottontail::minfinity;
    while (!done) {
      done = stop;
      warren->start();
      std::unique_ptr<cottontail::Hopper> h = warren->hopper_from_gcl(query);
      cottontail::addr p, q, v;
      h->uat(cottontail::maxfinity - 1, &p, &q, &v);
      if (p > last) {
        last = p;
        std::vector<std::string> tokens =
            warren->tokenizer()->split(warren->txt()->translate(p, q));
        ASSERT_EQ(tokens.size(), 1);
        output_mutex.lock();
        if (v == 0)
          std::cout << p << ", " << q << ": " << tokens[0] << "\n"
                    << std::flush;
        else
          std::cout << p << ", " << q << ", " << v << ": " << query << "\n"
                    << std::flush;
        output_mutex.unlock();
      }
      warren->end();
    }
  };

#if 1
  std::vector<std::thread> searchers;
  searchers.emplace_back(std::thread(search_worker, "tf:it"));
  searchers.emplace_back(std::thread(search_worker, "black"));
#endif

#if 0
  std::vector<std::thread> searchers;
  std::string black = "tf:" + stemmer->stem("black");
  searchers.emplace_back(std::thread(search_worker, black));
  std::string bear = "tf:" + stemmer->stem("bear");
  searchers.emplace_back(std::thread(search_worker, bear));
  std::string attacks = "tf:" + stemmer->stem("attacks");
  searchers.emplace_back(std::thread(search_worker, attacks));
#endif

  std::vector<std::thread> scribers;
  for (size_t i = 0; i < threads; i++)
    scribers.emplace_back(std::thread(scribe_worker));
  for (auto &scriber : scribers)
    scriber.join();
  stop = true;

#if 1
  for (auto &searcher : searchers)
    searcher.join();
#endif

  return 0;
}
