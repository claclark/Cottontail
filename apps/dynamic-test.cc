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

#define ASSERT_NE(a, b) (assert((a) != (b)))
#define ASSERT_TRUE(a) (assert((a)))

constexpr int THREADS = 50;

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
  size_t threads = THREADS;
  if (argc > 2 &&
      (argv[1] == std::string("-t") || argv[1] == std::string("--threads"))) {
    try {
      int t = std::stoi(argv[2]);
      if (t > 0 && t < THREADS)
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

  std::mutex docq_mutex;
  std::mutex output_mutex;
  auto worker = [&]() {
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
      ASSERT_TRUE(
          cottontail::scribe_files(doc, cottontail::Scribe::make(warren)));
      output_mutex.lock();
      std::cout << doc[0] << "\n";
      output_mutex.unlock();
    }
  };
  std::vector<std::thread> workers;
  for (size_t i = 0; i < threads; i++)
    workers.emplace_back(std::thread(worker));
  for (auto &worker : workers)
    worker.join();

std::cout << "hello\n";
  sleep(2);
std::cout << "world\n";
#if 0
  bigwig->start();
  std::unique_ptr<cottontail::Hopper> h =
      bigwig->hopper_from_gcl("(... \"<DOCNO>\" \"</DOCNO>\")");
  ASSERT_NE(h, nullptr);
  cottontail::addr p, q;
  for (h->tau(cottontail::minfinity + 1, &p, &q); p < cottontail::maxfinity;
       h->tau(p + 1, &p, &q))
    std::cout << bigwig->txt()->translate(p, q) << "\n";
  bigwig->end();
#endif

  std::shared_ptr<cottontail::Warren> warren = bigwig->clone();
  warren->start();
  ASSERT_TRUE(cottontail::tf_df_annotations(warren));
  warren->end();

  return 0;
}
