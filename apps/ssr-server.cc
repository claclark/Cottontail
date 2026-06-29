#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "src/cottontail.h"
#include "src/nlohmann.h"

namespace {

constexpr cottontail::addr WINDOW_TOKENS = 200;
constexpr size_t DEPTH = 1000;

struct Collection {
  std::string burrow;
  std::shared_ptr<cottontail::Warren> warren;
};

struct Result {
  size_t collection = 0;
  cottontail::RankingResult ranking;
  std::string docno;
};

struct QueryState {
  std::vector<Result> results;
  size_t next = 0;
};

void usage(const std::string &program_name) {
  std::cerr << "usage: " << program_name
            << " port container content docno burrow [burrow...]\n";
}

std::string clean_text(std::string text) {
  std::replace(text.begin(), text.end(), '\n', ' ');
  std::replace(text.begin(), text.end(), '\r', ' ');
  return text;
}

std::string translate(std::shared_ptr<cottontail::Warren> warren,
                      cottontail::addr p, cottontail::addr q) {
  return clean_text(warren->txt()->translate(p, q));
}

std::string highlighted(std::shared_ptr<cottontail::Warren> warren,
                        cottontail::addr start, cottontail::addr end,
                        cottontail::addr highlight_start,
                        cottontail::addr highlight_end) {
  if (highlight_start < start || highlight_end > end ||
      highlight_start > highlight_end)
    return translate(warren, start, end);
  std::string text;
  if (start < highlight_start)
    text += translate(warren, start, highlight_start - 1);
  text += "<cover>";
  text += translate(warren, highlight_start, highlight_end);
  text += "</cover>";
  if (highlight_end < end)
    text += translate(warren, highlight_end + 1, end);
  return text;
}

std::string snippet(std::shared_ptr<cottontail::Warren> warren,
                    const Result &result) {
  cottontail::addr cp = result.ranking.container_p();
  cottontail::addr cq = result.ranking.container_q();
  cottontail::addr p = result.ranking.p();
  cottontail::addr q = result.ranking.q();
  if (cq - cp + 1 <= WINDOW_TOKENS)
    return highlighted(warren, cp, cq, p, q);
  if (q - p + 1 >= WINDOW_TOKENS) {
    cottontail::addr end = p + WINDOW_TOKENS - 1;
    if (end > q)
      end = q;
    if (end > cq)
      end = cq;
    return highlighted(warren, p, end, p, end);
  }
  cottontail::addr extra = WINDOW_TOKENS - (q - p + 1);
  cottontail::addr start = p - extra / 2;
  cottontail::addr end = q + extra - extra / 2;
  if (start < cp) {
    end += cp - start;
    start = cp;
  }
  if (end > cq) {
    start -= end - cq;
    end = cq;
    if (start < cp)
      start = cp;
  }
  return highlighted(warren, start, end, p, q);
}

std::unique_ptr<cottontail::Hopper>
hopper(std::shared_ptr<cottontail::Warren> warren, const std::string &gcl,
       const std::string &name, std::string *error) {
  std::unique_ptr<cottontail::Hopper> h = warren->hopper_from_gcl(gcl, error);
  if (h == nullptr && error != nullptr && error->empty())
    *error = "Cannot create hopper for " + name;
  return h;
}

bool container_for(std::shared_ptr<cottontail::Warren> warren,
                   const std::string &container,
                   const cottontail::RankingResult &ranking,
                   cottontail::addr *cp, cottontail::addr *cq,
                   std::string *error) {
  std::unique_ptr<cottontail::Hopper> chopper =
      hopper(warren, container, "container", error);
  if (chopper == nullptr)
    return false;
  chopper->rho(ranking.q(), cp, cq);
  return *cp <= ranking.p() && ranking.q() <= *cq;
}

bool docno_in(std::shared_ptr<cottontail::Warren> warren,
              const std::string &docno_query, cottontail::addr cp,
              cottontail::addr cq, std::string *docno, std::string *error) {
  std::unique_ptr<cottontail::Hopper> dhopper =
      hopper(warren, docno_query, "docno", error);
  if (dhopper == nullptr)
    return false;
  cottontail::addr dp, dq;
  dhopper->tau(cp, &dp, &dq);
  if (dq > cq)
    return false;
  *docno = warren->txt()->translate(dp, dq);
  return true;
}

bool make_result(const Collection &collection, size_t collection_index,
                 const std::string &container, const std::string &docno_query,
                 const cottontail::RankingResult &ranking, Result *result) {
  std::string error;
  cottontail::addr cp, cq;
  if (!container_for(collection.warren, container, ranking, &cp, &cq, &error)) {
    if (!error.empty())
      std::cerr << "ssr-server: " << collection.burrow << ": " << error
                << "\n";
    return false;
  }
  std::string docno;
  if (!docno_in(collection.warren, docno_query, cp, cq, &docno, &error)) {
    if (!error.empty())
      std::cerr << "ssr-server: " << collection.burrow << ": " << error
                << "\n";
    return false;
  }
  result->collection = collection_index;
  result->ranking = ranking;
  result->docno = docno;
  return true;
}

std::vector<size_t> thread_budget(size_t collections) {
  std::vector<size_t> budget(collections, 2);
  size_t allowed = cottontail::allowed_threads(0);
  size_t base = 2 * collections;
  if (collections == 0 || allowed <= base)
    return budget;
  size_t extra = allowed - base;
  for (size_t i = 0; i < collections; i++)
    budget[i] += extra / collections + (i < extra % collections ? 1 : 0);
  return budget;
}

json result_response(const std::string &op, const std::string &qid,
                     size_t rank, const Collection &collection,
                     const Result &result) {
  json response;
  response["op"] = op;
  response["ok"] = true;
  response["qid"] = qid;
  response["rank"] = rank;
  response["burrow"] = collection.burrow;
  response["docno"] = result.docno;
  response["snippet"] = snippet(collection.warren, result);
  return response;
}

json error_response(const std::string &op, const std::string &error) {
  json response;
  response["op"] = op;
  response["ok"] = false;
  response["error"] = error;
  return response;
}

ssize_t write_all(int fd, const std::string &text) {
  const char *p = text.data();
  size_t n = text.size();
  while (n > 0) {
    ssize_t wrote = send(fd, p, n, 0);
    if (wrote <= 0)
      return wrote;
    p += wrote;
    n -= static_cast<size_t>(wrote);
  }
  return static_cast<ssize_t>(text.size());
}

bool send_record(int fd, const json &request, const json &response,
                 cottontail::addr time) {
  json record;
  record["query"] = request;
  record["response"] = response;
  record["time"] = time;
  std::string line = record.dump() + "\n";
  std::cout << line << std::flush;
  return write_all(fd, line) > 0;
}

bool read_line(int fd, std::string *line) {
  line->clear();
  for (;;) {
    char c;
    ssize_t n = recv(fd, &c, 1, 0);
    if (n == 0)
      return !line->empty();
    if (n < 0)
      return false;
    if (c == '\n')
      return true;
    if (c != '\r')
      line->push_back(c);
  }
}

int listen_local(uint16_t port, uint16_t *actual_port) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    return -1;
  int yes = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  sockaddr_in address;
  std::memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = htons(port);
  if (bind(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0) {
    close(fd);
    return -1;
  }
  if (listen(fd, 1) != 0) {
    close(fd);
    return -1;
  }
  socklen_t length = sizeof(address);
  if (getsockname(fd, reinterpret_cast<sockaddr *>(&address), &length) != 0) {
    close(fd);
    return -1;
  }
  *actual_port = ntohs(address.sin_port);
  return fd;
}

class Server {
public:
  Server(std::string container, std::string content, std::string docno,
         std::vector<Collection> collections)
      : container_(container), content_(content), docno_(docno),
        collections_(collections) {}

  void handle(int fd) {
    std::string line;
    while (read_line(fd, &line)) {
      cottontail::addr start = cottontail::now();
      json request;
      json response;
      try {
        request = json::parse(line);
      } catch (json::parse_error &e) {
        request = json::object();
        request["op"] = "parse";
        response = error_response("parse", e.what());
        send_record(fd, request, response, cottontail::now() - start);
        continue;
      }
      std::string op = request.value("op", "");
      if (op == "query")
        response = query(request);
      else if (op == "next")
        response = next(request);
      else if (op == "document")
        response = document(request);
      else
        response = error_response(op, "Unknown op");
      if (!send_record(fd, request, response, cottontail::now() - start))
        return;
    }
  }

private:
  json query(const json &request) {
    std::string query = request.value("query", "");
    if (query.empty())
      return error_response("query", "Missing query");
    std::vector<size_t> threads = thread_budget(collections_.size());
    std::vector<std::vector<Result>> per_collection(collections_.size());
    std::vector<std::thread> workers;
    workers.reserve(collections_.size());
    std::mutex sync;
    std::string first_error;
    for (size_t i = 0; i < collections_.size(); i++) {
      workers.emplace_back(std::thread([&, i] {
        std::string error;
        if (collections_[i].warren->hopper_from_gcl(query, &error) == nullptr) {
          std::lock_guard<std::mutex> _(sync);
          if (first_error.empty())
            first_error = error.empty() ? "Cannot parse query" : error;
          return;
        }
        auto ranking = cottontail::parallel_ssr(collections_[i].warren, query,
                                               content_, DEPTH, threads[i]);
        for (auto &ranked : ranking) {
          Result result;
          if (make_result(collections_[i], i, container_, docno_, ranked,
                          &result))
            per_collection[i].push_back(result);
        }
      }));
    }
    for (auto &worker : workers)
      worker.join();
    if (!first_error.empty())
      return error_response("query", first_error);
    std::vector<Result> merged;
    for (auto &results : per_collection)
      for (auto &result : results)
        merged.push_back(result);
    std::stable_sort(merged.begin(), merged.end(),
                     [](const Result &a, const Result &b) {
                       return a.ranking.score() > b.ranking.score();
                     });
    std::string qid = "q" + std::to_string(next_qid_++);
    QueryState state;
    state.results = merged;
    queries_[qid] = state;
    if (queries_[qid].results.empty()) {
      json response;
      response["op"] = "query";
      response["ok"] = true;
      response["qid"] = qid;
      response["done"] = true;
      return response;
    }
    Result result = queries_[qid].results[0];
    queries_[qid].next = 1;
    return result_response("query", qid, 1, collections_[result.collection],
                           result);
  }

  json next(const json &request) {
    std::string qid = request.value("qid", "");
    auto found = queries_.find(qid);
    if (found == queries_.end())
      return error_response("next", "Unknown qid");
    QueryState &state = found->second;
    if (state.next >= state.results.size()) {
      json response;
      response["op"] = "next";
      response["ok"] = true;
      response["qid"] = qid;
      response["done"] = true;
      return response;
    }
    size_t index = state.next++;
    Result result = state.results[index];
    return result_response("next", qid, index + 1,
                           collections_[result.collection], result);
  }

  json document(const json &request) {
    std::string wanted = request.value("docno", "");
    if (wanted.empty())
      return error_response("document", "Missing docno");
    for (auto &collection : collections_) {
      std::string error;
      std::unique_ptr<cottontail::Hopper> dhopper =
          hopper(collection.warren, docno_, "docno", &error);
      if (dhopper == nullptr)
        continue;
      for (cottontail::addr dp, dq,
           k = cottontail::minfinity + 1;
           k < cottontail::maxfinity; k = dp + 1) {
        dhopper->tau(k, &dp, &dq);
        if (dp >= cottontail::maxfinity)
          break;
        std::string docno = collection.warren->txt()->translate(dp, dq);
        if (docno != wanted)
          continue;
        cottontail::RankingResult cover(dp, dq, 0.0);
        cottontail::addr cp, cq;
        if (!container_for(collection.warren, container_, cover, &cp, &cq,
                           &error))
          continue;
        json response;
        response["op"] = "document";
        response["ok"] = true;
        response["burrow"] = collection.burrow;
        response["docno"] = wanted;
        response["document"] = clean_text(collection.warren->txt()->translate(
            cp, cq));
        return response;
      }
    }
    return error_response("document", "Unknown docno");
  }

  std::string container_;
  std::string content_;
  std::string docno_;
  std::vector<Collection> collections_;
  std::map<std::string, QueryState> queries_;
  size_t next_qid_ = 1;
};

} // namespace

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  if (argc < 6) {
    usage(program_name);
    return 1;
  }
  uint16_t requested_port =
      static_cast<uint16_t>(std::strtoul(argv[1], nullptr, 10));
  std::string container = argv[2];
  std::string content = argv[3];
  std::string docno = argv[4];
  std::vector<Collection> collections;
  for (int i = 5; i < argc; i++) {
    std::string error;
    std::string burrow = argv[i];
    std::shared_ptr<cottontail::Warren> warren =
        cottontail::Warren::make(burrow, &error);
    if (warren == nullptr) {
      std::cerr << program_name << ": " << burrow << ": " << error << "\n";
      return 1;
    }
    warren->start();
    if (warren->hopper_from_gcl(container, &error) == nullptr ||
        warren->hopper_from_gcl(content, &error) == nullptr ||
        warren->hopper_from_gcl(docno, &error) == nullptr) {
      std::cerr << program_name << ": " << burrow << ": " << error << "\n";
      warren->end();
      return 1;
    }
    collections.push_back({burrow, warren});
  }
  uint16_t actual_port = 0;
  int server = listen_local(requested_port, &actual_port);
  if (server < 0) {
    std::cerr << program_name << ": cannot listen on 127.0.0.1:"
              << requested_port << ": " << std::strerror(errno) << "\n";
    return 1;
  }
  std::cerr << program_name << ": listening on 127.0.0.1:" << actual_port
            << "\n";
  Server ssr(container, content, docno, collections);
  for (;;) {
    sockaddr_in client_address;
    socklen_t length = sizeof(client_address);
    int client =
        accept(server, reinterpret_cast<sockaddr *>(&client_address), &length);
    if (client < 0) {
      std::cerr << program_name << ": accept failed: " << std::strerror(errno)
                << "\n";
      continue;
    }
    std::cerr << program_name << ": client connected\n";
    ssr.handle(client);
    close(client);
    std::cerr << program_name << ": client closed\n";
  }
}
