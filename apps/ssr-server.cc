#include <algorithm>
#include <cerrno>
#include <cctype>
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

struct LocatedDocument {
  size_t collection = 0;
  cottontail::addr container_p = cottontail::maxfinity;
  cottontail::addr container_q = cottontail::minfinity;
};

struct QueryState {
  std::vector<Result> results;
  size_t next = 0;
};

void usage(const std::string &program_name) {
  std::cerr << "usage: " << program_name << " [--fields fields] "
            << "container content docno burrow [burrow...]\n";
}

std::string trim(const std::string &text) {
  size_t begin = 0;
  while (begin < text.size() &&
         std::isspace(static_cast<unsigned char>(text[begin])))
    begin++;
  size_t end = text.size();
  while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])))
    end--;
  return text.substr(begin, end - begin);
}

bool split_fields(const std::string &spec, std::vector<std::string> *fields,
                  std::string *error) {
  if (spec.empty()) {
    *error = "--fields cannot be empty";
    return false;
  }
  fields->clear();
  size_t begin = 0;
  for (;;) {
    size_t end = spec.find(',', begin);
    std::string field =
        trim(spec.substr(begin, end == std::string::npos ? end : end - begin));
    if (field.empty()) {
      *error = "--fields contains an empty field query";
      return false;
    }
    fields->push_back(field);
    if (end == std::string::npos)
      return true;
    begin = end + 1;
  }
}

std::string clean_text(std::string text) {
  std::replace(text.begin(), text.end(), '\n', ' ');
  std::replace(text.begin(), text.end(), '\r', ' ');
  return text;
}

std::string double_quote_if_needed(const std::string &text) {
  std::string value = text;
  if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
    value = value.substr(1, value.size() - 2);
  std::string quoted = "\"";
  for (char c : value) {
    if (c == '\\' || c == '"')
      quoted.push_back('\\');
    quoted.push_back(c);
  }
  quoted.push_back('"');
  return quoted;
}

std::string strip_outer_quotes(const std::string &text) {
  if (text.size() >= 2 && text.front() == '"' && text.back() == '"')
    return text.substr(1, text.size() - 2);
  return text;
}

std::string translate(std::shared_ptr<cottontail::Warren> warren,
                      cottontail::addr p, cottontail::addr q) {
  return clean_text(warren->txt()->translate(p, q));
}

std::string display_docno(const std::string &text) {
  return cottontail::trec_docno(cottontail::json_translate(text));
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
  *docno = display_docno(warren->txt()->translate(dp, dq));
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
         std::vector<std::string> fields, std::vector<Collection> collections)
      : container_(container), content_(content), docno_(docno),
        fields_(fields), collections_(collections) {}

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
    std::string error;
    LocatedDocument document;
    if (!locate_document(wanted, &document, &error))
      return error_response("document", error);
    const Collection &collection = collections_[document.collection];
    std::string text;
    if (!document_text(collection, document.container_p, document.container_q,
                       &text, &error))
      return error_response("document", error);
    json response;
    response["op"] = "document";
    response["ok"] = true;
    response["burrow"] = collection.burrow;
    response["docno"] = wanted;
    response["document"] = text;
    return response;
  }

  bool locate_document(const std::string &wanted, LocatedDocument *document,
                       std::string *error) {
    std::string query = "(>> " + container_ + " (>> " + docno_ + " " +
                        double_quote_if_needed(wanted) + "))";
    size_t matches = 0;
    for (size_t i = 0; i < collections_.size(); i++) {
      std::string hopper_error;
      std::unique_ptr<cottontail::Hopper> h =
          hopper(collections_[i].warren, query, "document", &hopper_error);
      if (h == nullptr) {
        *error = hopper_error;
        return false;
      }
      cottontail::addr p, q;
      h->tau(cottontail::minfinity + 1, &p, &q);
      while (p < cottontail::maxfinity) {
        if (matches == 0)
          *document = LocatedDocument{i, p, q};
        matches++;
        if (matches > 1) {
          std::cerr << "ssr-server: ambiguous docno: " << wanted << "\n";
          *error = "Ambiguous docno";
          return false;
        }
        h->tau(p + 1, &p, &q);
      }
    }
    if (matches == 0) {
      *error = "Unknown docno";
      return false;
    }
    return true;
  }

  bool document_text(const Collection &collection, cottontail::addr cp,
                     cottontail::addr cq, std::string *text,
                     std::string *error) {
    if (fields_.empty()) {
      *text = clean_text(collection.warren->txt()->translate(cp, cq));
      return true;
    }
    text->clear();
    for (auto &field : fields_) {
      std::string hopper_error;
      std::unique_ptr<cottontail::Hopper> h =
          hopper(collection.warren, field, "field", &hopper_error);
      if (h == nullptr) {
        *error = hopper_error;
        return false;
      }
      cottontail::addr p, q;
      h->tau(cp, &p, &q);
      while (p < cottontail::maxfinity && p <= cq) {
        if (q <= cq) {
          if (!text->empty())
            *text += " ... ";
          *text += strip_outer_quotes(translate(collection.warren, p, q));
        }
        h->tau(p + 1, &p, &q);
      }
    }
    *text = clean_text(*text);
    return true;
  }

  std::string container_;
  std::string content_;
  std::string docno_;
  std::vector<std::string> fields_;
  std::vector<Collection> collections_;
  std::map<std::string, QueryState> queries_;
  size_t next_qid_ = 0;
};

} // namespace

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  std::vector<std::string> arguments;
  std::string field_spec;
  bool saw_fields = false;
  for (int i = 1; i < argc; i++) {
    std::string argument = argv[i];
    if (argument == "--help") {
      usage(program_name);
      return 0;
    } else if (argument == "--fields") {
      if (++i >= argc) {
        std::cerr << program_name << ": missing --fields value\n";
        return 1;
      }
      field_spec = argv[i];
      saw_fields = true;
    } else {
      arguments.push_back(argument);
    }
  }
  if (arguments.size() < 4) {
    usage(program_name);
    return 1;
  }
  std::vector<std::string> fields;
  if (saw_fields) {
    std::string error;
    if (!split_fields(field_spec, &fields, &error)) {
      std::cerr << program_name << ": " << error << "\n";
      return 1;
    }
  }
  std::string container = arguments[0];
  std::string content = arguments[1];
  std::string docno = arguments[2];
  std::vector<Collection> collections;
  for (size_t i = 3; i < arguments.size(); i++) {
    std::string error;
    std::string burrow = arguments[i];
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
    for (auto &field : fields) {
      if (warren->hopper_from_gcl(field, &error) == nullptr) {
        std::cerr << program_name << ": " << burrow << ": " << error << "\n";
        warren->end();
        return 1;
      }
    }
    collections.push_back({burrow, warren});
  }
  uint16_t actual_port = 0;
  int server = listen_local(0, &actual_port);
  if (server < 0) {
    std::cerr << program_name << ": cannot listen: " << std::strerror(errno)
              << "\n";
    return 1;
  }
  std::cerr << program_name << ": listening on port " << actual_port << "\n";
  Server ssr(container, content, docno, fields, collections);
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
