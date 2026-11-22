#include "meadowlark/server.h"

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <filesystem>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>

#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#ifdef __linux__
#include <signal.h>
#include <sys/syscall.h>
#include <wordexp.h>
#endif

#include "meadowlark/meadowlark.h"
#include "meadowlark/remote_protocol.h"
#include "src/core.h"
#include "src/dna.h"
#include "src/json.h"
#include "src/recipe.h"
#include "src/warren.h"
#include "src/wlock.h"

namespace cottontail {
namespace meadowlark {
namespace {

using namespace remote_protocol;

class Descriptor {
public:
  explicit Descriptor(int fd) : fd_(fd) {}
  ~Descriptor() {
    if (fd_ >= 0)
      close(fd_);
  }
  int get() const { return fd_; }
  Descriptor(const Descriptor &) = delete;
  Descriptor &operator=(const Descriptor &) = delete;

private:
  int fd_;
};

bool ensure_output_stream(int fd, std::string *error) {
  if (fcntl(fd, F_GETFD) >= 0)
    return true;
  int placeholder = open("/dev/null", O_WRONLY);
  if (placeholder == fd)
    return true;
  Descriptor output(placeholder);
  if (output.get() >= 0 && dup2(output.get(), fd) >= 0)
    return true;
  safe_error(error) = "Cannot reserve a server output descriptor: " +
                      std::string(std::strerror(errno));
  return false;
}

bool redirect_log(const std::string &filename, std::string *error) {
  Descriptor logfile(open(filename.c_str(),
                          O_WRONLY | O_CREAT | O_APPEND | O_NOFOLLOW |
                              O_NONBLOCK | O_CLOEXEC,
                          0666));
  struct stat status;
  if (logfile.get() < 0 || fstat(logfile.get(), &status) != 0 ||
      !S_ISREG(status.st_mode)) {
    safe_error(error) = "Cannot open Meadowlark log: " + filename;
    return false;
  }
  if (dup2(logfile.get(), STDOUT_FILENO) < 0 ||
      dup2(logfile.get(), STDERR_FILENO) < 0) {
    safe_error(error) = "Cannot redirect Meadowlark output: " +
                        std::string(std::strerror(errno));
    return false;
  }
  return true;
}

bool listen_local(int listener, std::string *url, std::string *error) {
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  socklen_t size = sizeof(address);
  sockaddr *socket_address = reinterpret_cast<sockaddr *>(&address);
  if (listener < 0 || bind(listener, socket_address, size) != 0 ||
      listen(listener, SOMAXCONN) != 0 ||
      getsockname(listener, socket_address, &size) != 0) {
    safe_error(error) = "Cannot listen on loopback: " +
                        std::string(std::strerror(errno));
    return false;
  }
  *url = "http://127.0.0.1:" + std::to_string(ntohs(address.sin_port)) + "/";
  return true;
}

bool publish_url(Working &working, const std::string &filename,
                 const std::string &url, std::string *error) {
  std::string temporary = working.make_temp("url");
  std::ofstream output(temporary);
  if (!output) {
    safe_error(error) = "Cannot write server URL: " + temporary;
    return false;
  }
  output << url << "\n";
  output.close();
  if (output && std::rename(temporary.c_str(), filename.c_str()) == 0)
    return true;
  std::remove(temporary.c_str());
  safe_error(error) = "Cannot publish server URL: " + filename;
  return false;
}

ssize_t receive(int fd, char *buffer, size_t size) {
  ssize_t count;
  do {
    count = recv(fd, buffer, size, 0);
  } while (count < 0 && errno == EINTR);
  return count;
}

void send_response(Socket &socket, std::string_view status, std::string_view type,
                   const std::string &body) {
  std::ostringstream header;
  header << "HTTP/1.1 " << status << "\r\nContent-Type: " << type
         << "\r\nContent-Length: " << body.size()
         << "\r\nConnection: close\r\nCache-Control: no-store\r\n\r\n";
  socket.write_bytes(header.str() + body);
}

std::string html_escape(std::string_view text) {
  std::string escaped;
  for (char ch : text)
    switch (ch) {
    case '&':
      escaped += "&amp;";
      break;
    case '<':
      escaped += "&lt;";
      break;
    case '>':
      escaped += "&gt;";
      break;
    case '"':
      escaped += "&quot;";
      break;
    default:
      escaped += ch;
    }
  return escaped;
}

int hex_digit(char ch) {
  if (ch >= '0' && ch <= '9')
    return ch - '0';
  if (ch >= 'A' && ch <= 'F')
    return ch - 'A' + 10;
  if (ch >= 'a' && ch <= 'f')
    return ch - 'a' + 10;
  return -1;
}

bool decode_query(std::string_view encoded, std::string *query) {
  for (size_t i = 0; i < encoded.size(); i++) {
    char ch = encoded[i];
    if (ch == '+') {
      *query += ' ';
    } else if (ch == '%') {
      if (encoded.size() - i < 3)
        return false;
      int high = hex_digit(encoded[i + 1]), low = hex_digit(encoded[i + 2]);
      if (high < 0 || low < 0 || (high == 0 && low == 0))
        return false;
      *query += static_cast<char>(16 * high + low);
      i += 2;
    } else {
      *query += ch;
    }
  }
  return true;
}

class ReadEpoch {
public:
  explicit ReadEpoch(Warren &warren) : warren_(warren) { warren_.start(); }
  ~ReadEpoch() { warren_.end(); }
  ReadEpoch(const ReadEpoch &) = delete;
  ReadEpoch &operator=(const ReadEpoch &) = delete;

private:
  Warren &warren_;
};

std::string inspect(Warren &original, const std::string &query) {
  std::string error;
  auto warren = original.clone(&error);
  if (warren == nullptr)
    return error;
  ReadEpoch epoch(*warren);
  auto hopper = warren->hopper_from_gcl(query, &error);
  if (hopper == nullptr)
    return error;
  auto txt = warren->txt();
  auto snippet = [&txt](addr p, addr q) {
    std::string text = txt->translate(p, q);
    std::replace(text.begin(), text.end(), '\n', ' ');
    std::replace(text.begin(), text.end(), '\r', ' ');
    return json_translate(text);
  };
  constexpr addr window = 128;
  std::ostringstream matches;
  addr p, q, next = minfinity + 1;
  fval value;
  for (unsigned i = 0; i < 24; i++) {
    hopper->tau(next, &p, &q, &value);
    if (p >= maxfinity)
      break;
    matches << "(" << p << "," << q << ")";
    addr annotation = fval2addr(value);
    if (annotation != 0)
      matches << annotation;
    matches << ": ";
    if (q - p <= window)
      matches << snippet(p, q);
    else
      matches << snippet(p, p + window / 2) << "... "
              << snippet(q - window / 2, q);
    matches << "\n";
    next = p + 1;
  }
  std::string result = matches.str();
  return result.empty() ? "No matches." : result;
}

std::string page(Warren *warren, const std::string &query) {
  std::ostringstream html;
  html << R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Meadowlark Inspect</title>
<style>
  body { max-width: 900px; margin: 3rem auto; padding: 0 1.5rem;
         font: 16px/1.5 system-ui, sans-serif; }
  form { display: flex; gap: .6rem; align-items: baseline; }
  input { flex: 1; min-width: 0; font: inherit; padding: .35em; }
  button { font: inherit; }
  pre { padding: 1rem; background: #f4f4f4; white-space: pre-wrap;
        overflow-wrap: anywhere; }
</style>
)HTML";
  if (warren == nullptr) {
    html << "<meta http-equiv=\"refresh\" content=\"2\">\n"
         << "</head><body><h1>Meadowlark Inspect</h1>"
         << "<p>Loading Warren…</p>";
  } else {
    html << "</head><body><h1>Meadowlark Inspect</h1>\n"
         << "<form action=\"/\" method=\"get\">"
         << "<label for=\"q\">GCL query</label>"
         << "<input id=\"q\" name=\"q\" autocomplete=\"off\" autofocus value=\""
         << html_escape(query)
         << "\"><button type=\"submit\">Run</button></form>\n";
    if (!query.empty())
      html << "<pre>" << html_escape(inspect(*warren, query)) << "</pre>\n";
  }
  html << "</body></html>\n";
  return html.str();
}

json result(json value) { return {{"response", std::move(value)}}; }

json failure(const std::string &error) { return {{"error", error}}; }

json outcome(bool success, const std::string &error) {
  return success ? result(true)
                 : failure(error.empty() ? "Warren operation failed" : error);
}

class Session {
public:
  explicit Session(std::shared_ptr<Warren> warren) : warren_(std::move(warren)) {}
  ~Session() { close(); }

  void close() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (warren_ == nullptr)
      return;
    warren_->abort();
    hoppers_.clear();
    warren_->end();
    warren_.reset();
  }

  std::shared_ptr<Warren> clone(std::string *error) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (warren_ != nullptr)
      return warren_->clone(error);
    safe_error(error) = "Source Meadowlark session is closed";
    return nullptr;
  }

  json dispatch(const json &request) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto component = string_field(request, "component");
    auto method = string_field(request, "method");
    if (component == nullptr || method == nullptr)
      return failure("Expected component and method");
    if (*component == "warren")
      return warren_request(*method, request);
    if (*component == "featurizer")
      return featurizer_request(*method, request);
    if (*component == "hopper")
      return hopper_request(*method, request);
    if (*component == "idx" || *component == "txt") {
      if (!warren_->started())
        return failure("Warren has not been started");
      return *component == "idx" ? idx_request(*method, request)
                                  : txt_request(*method, request);
    }
    if (*component == "appender" || *component == "annotator")
      return write_request(*component, *method, request);
    return failure("Unknown component");
  }

private:
  json add_hopper(std::unique_ptr<Hopper> hopper, const std::string &error) {
    if (hopper == nullptr)
      return outcome(false, error);
    std::string handle = std::to_string(next_hopper_++);
    hoppers_.emplace(handle, std::move(hopper));
    return {{"hopper", handle}};
  }

  json warren_request(const std::string &method, const json &request) {
    if (method == "started")
      return result(warren_->started());
    if (method == "start") {
      if (warren_->started())
        return failure("Warren is already started");
      warren_->start();
      return result(true);
    }
    if (method == "end") {
      warren_->end();
      return result(true);
    }
    std::string error;
    if (method == "hopper_from_gcl") {
      auto query = string_field(request, "query");
      if (query == nullptr)
        return failure("Expected a GCL query");
      if (!warren_->started())
        return failure("Warren has not been started");
      return add_hopper(warren_->hopper_from_gcl(*query, &error), error);
    }
    if (method == "transaction")
      return outcome(warren_->transaction(&error), error);
    if (method == "ready") {
      if (!warren_->transacting())
        return failure("Warren has no open transaction");
      readied_ = true;
      vote_ = warren_->ready(&error);
      return outcome(vote_, error);
    }
    if (method == "commit") {
      if (!warren_->transacting() || !vote_)
        return failure("Commit requires a successful ready");
      warren_->commit();
    } else if (method == "abort") {
      warren_->abort();
    } else {
      return failure("Unknown Warren method");
    }
    readied_ = vote_ = false;
    return result(true);
  }

  json featurizer_request(const std::string &method, const json &request) {
    auto featurizer = warren_->featurizer();
    if (featurizer == nullptr)
      return failure("Warren has no featurizer");
    if (method == "name")
      return result(featurizer->name());
    if (method == "recipe")
      return result(featurizer->recipe());
    if (method == "featurize") {
      auto label = string_field(request, "label");
      if (label == nullptr)
        return failure("Expected a feature label");
      return result(std::to_string(featurizer->featurize(*label)));
    }
    if (method == "translate") {
      addr feature;
      if (!addr_field(request, "feature", &feature))
        return failure("Expected a feature address");
      return result(featurizer->translate(feature));
    }
    return failure("Unknown featurizer method");
  }

  json idx_request(const std::string &method, const json &request) {
    auto idx = warren_->idx();
    if (idx == nullptr)
      return failure("Warren has no index");
    if (method == "vocab")
      return result(std::to_string(idx->vocab()));
    addr feature;
    if (!addr_field(request, "feature", &feature))
      return failure("Expected a feature address");
    if (method == "hopper")
      return add_hopper(idx->hopper(feature), "Could not create index hopper");
    if (method == "count")
      return result(std::to_string(idx->count(feature)));
    return failure("Unknown index method");
  }

  json txt_request(const std::string &method, const json &request) {
    auto txt = warren_->txt();
    if (txt == nullptr)
      return failure("Warren has no text");
    if (method == "tokens")
      return result(std::to_string(txt->tokens()));
    addr p, q;
    if (method == "range")
      return result(txt->range(&p, &q) ? interval(p, q) : json());
    if (method == "translate") {
      if (!addr_field(request, "p", &p) || !addr_field(request, "q", &q))
        return failure("Expected p and q addresses");
      return result(txt->translate(p, q));
    }
    return failure("Unknown text method");
  }

  json write_request(const std::string &component, const std::string &method,
                     const json &request) {
    if (!warren_->transacting() || readied_)
      return failure("Write requires an open transaction before ready");
    std::string error;
    addr p, q;
    if (component == "appender") {
      if (method != "append")
        return failure("Unknown appender method");
      auto text = string_field(request, "text");
      auto appender = warren_->appender();
      if (text == nullptr || appender == nullptr)
        return failure("Append requires text and a writable Warren");
      if (!appender->append(*text, &p, &q, &error))
        return outcome(false, error);
      return result(interval(p, q));
    }
    auto annotator = warren_->annotator();
    if (annotator == nullptr)
      return failure("Warren has no annotator");
    if (!addr_field(request, "p", &p) || !addr_field(request, "q", &q) ||
        p < 0 || p > q || q == maxfinity)
      return failure("Expected an interval of token addresses");
    if (method == "erase")
      return outcome(annotator->erase(p, q, &error), error);
    if (method == "annotate") {
      addr feature, value = 0;
      if (!addr_field(request, "feature", &feature) ||
          (request.contains("v") && !addr_field(request, "v", &value)))
        return failure("Expected a feature and optional annotation value");
      return outcome(annotator->annotate(feature, p, q, value, &error), error);
    }
    return failure("Unknown annotator method");
  }

  json hopper_request(const std::string &method, const json &request) {
    auto handle = string_field(request, "hopper");
    if (method == "complete") {
      if (handle != nullptr)
        hoppers_.erase(*handle);
      return {};
    }
    addr k;
    if (handle == nullptr || !addr_field(request, "k", &k))
      return failure("Expected a hopper handle and a seek address");
    auto entry = hoppers_.find(*handle);
    Hopper &hopper = entry == hoppers_.end() ? empty_ : *entry->second;
    if (method == "L")
      return result(std::to_string(hopper.L(k)));
    if (method == "R")
      return result(std::to_string(hopper.R(k)));
    addr p, q;
    fval value;
    if (method == "tau")
      hopper.tau(k, &p, &q, &value);
    else if (method == "rho")
      hopper.rho(k, &p, &q, &value);
    else if (method == "uat")
      hopper.uat(k, &p, &q, &value);
    else if (method == "ohr")
      hopper.ohr(k, &p, &q, &value);
    else
      return failure("Unknown hopper method");
    return result(interval(p, q, fval2addr(value)));
  }

  std::mutex mutex_;
  std::shared_ptr<Warren> warren_;
  std::map<std::string, std::unique_ptr<Hopper>> hoppers_;
  size_t next_hopper_ = 1;
  EmptyHopper empty_;
  bool readied_ = false;
  bool vote_ = false;
};

class Sessions {
public:
  std::string add(const std::shared_ptr<Session> &session) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string id;
    do {
      id = std::to_string(random_());
      for (int i = 0; i < 3; i++)
        id += "-" + std::to_string(random_());
    } while (!sessions_.emplace(id, session).second);
    return id;
  }

  std::shared_ptr<Session> find(const std::string &id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto entry = sessions_.find(id);
    return entry == sessions_.end() ? nullptr : entry->second.lock();
  }

  void remove(const std::string &id) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(id);
  }

private:
  std::mutex mutex_;
  std::random_device random_;
  std::map<std::string, std::weak_ptr<Session>> sessions_;
};

struct Activation {
  std::shared_ptr<Warren> warren;
  std::string error;
};

void serve_rpc(std::unique_ptr<Socket> client,
               std::shared_future<Activation> activation,
               std::shared_ptr<const json> hello,
               std::shared_ptr<Sessions> sessions) {
  std::shared_ptr<Session> session;
  std::string session_id;
  try {
    json request;
    if (!client->read(&request) || !greeting(request))
      return;
    std::shared_ptr<Warren> warren;
    std::string error;
    if (request.contains("clone_of")) {
      auto source_id = string_field(request, "clone_of");
      if (source_id == nullptr || source_id->empty()) {
        client->write(failure("Expected a source Meadowlark session ID"));
        return;
      }
      auto source = sessions->find(*source_id);
      if (source == nullptr) {
        client->write(failure("Source Meadowlark session is unavailable"));
        return;
      }
      warren = source->clone(&error);
    } else {
      const auto &original = activation.get().warren;
      if (original == nullptr)
        return;
      warren = original->clone(&error);
    }
    if (warren == nullptr) {
      client->write(failure(error.empty() ? "Cannot clone Meadowlark Warren"
                                          : error));
      return;
    }
    json greeting_reply = *hello;
    greeting_reply["started"] = warren->started();
    session = std::make_shared<Session>(std::move(warren));
    session_id = sessions->add(session);
    greeting_reply["session"] = session_id;
    if (client->write(greeting_reply) && client->timeout(0)) {
      while (client->read(&request)) {
        json reply = session->dispatch(request);
        if (reply.is_null())
          continue;
        try {
          if (!client->write(reply))
            break;
        } catch (const json::type_error &) {
          if (!client->write(failure("Response contains invalid UTF-8")))
            break;
        }
      }
    }
  } catch (const std::exception &exception) {
    std::cerr << "Meadowlark session failed: " << exception.what() << '\n';
  }
  if (!session_id.empty())
    sessions->remove(session_id);
  if (session != nullptr)
    session->close();
}

void serve_connection(std::unique_ptr<Socket> client,
                      const std::shared_ptr<Warren> &warren,
                      const std::shared_future<Activation> &activation,
                      const std::shared_ptr<const json> &hello,
                      const std::shared_ptr<Sessions> &sessions) {
  int fd = client->fd();
  if (!client->configure())
    return;
  char first;
  if (receive(fd, &first, 1) != 1)
    return;
  if (first == '\0') {
    std::thread(serve_rpc, std::move(client), activation, hello, sessions)
        .detach();
    return;
  }
  std::string header(1, first);
  constexpr size_t header_limit = 16 * 1024;
  while (header.find("\r\n\r\n") == std::string::npos) {
    char buffer[2048];
    size_t available = std::min(sizeof(buffer), header_limit - header.size());
    if (available == 0) {
      send_response(*client, "431 Request Header Fields Too Large", "text/plain",
                    "HTTP request header is too large.\n");
      return;
    }
    ssize_t count = receive(fd, buffer, available);
    if (count <= 0)
      return;
    header.append(buffer, count);
  }
  std::istringstream request(header.substr(0, header.find("\r\n")));
  std::string method, target, version;
  if (!(request >> method >> target >> version) || method != "GET" ||
      (version != "HTTP/1.1" && version != "HTTP/1.0")) {
    send_response(*client, "400 Bad Request", "text/plain",
                  "Expected an HTTP GET.\n");
    return;
  }
  if (target != "/" && !target.starts_with("/?q=")) {
    send_response(*client, "404 Not Found", "text/plain", "Not found.\n");
    return;
  }
  std::string query;
  if (target.starts_with("/?q=") &&
      !decode_query(std::string_view(target).substr(4), &query)) {
    send_response(*client, "400 Bad Request", "text/plain",
                  "Invalid query encoding.\n");
    return;
  }
  send_response(*client, "200 OK", "text/html; charset=utf-8",
                page(warren.get(), query));
}

bool serve(int listener, std::shared_ptr<WLock> wlock, std::string *error) {
  using namespace std::chrono_literals;
  std::string dna;
  if (!read_dna(wlock->working(), &dna, error))
    return false;
  auto hello = std::make_shared<const json>(
      json{{"protocol", protocol}, {"version", version}, {"dna", std::move(dna)}});
  auto sessions = std::make_shared<Sessions>();
  auto activation = std::async(std::launch::async, [wlock]() {
    Activation result;
    result.warren = Warren::local(wlock, &result.error);
    return result;
  }).share();
  std::shared_ptr<Warren> warren;
  for (;;) {
    if (warren == nullptr &&
        activation.wait_for(0ms) == std::future_status::ready) {
      const Activation &result = activation.get();
      if (result.warren == nullptr) {
        safe_set(error) = result.error;
        return false;
      }
      warren = result.warren;
      std::cerr << "Meadowlark Warren ready\n";
    }
    pollfd event{listener, POLLIN, 0};
    int ready = poll(&event, 1, warren == nullptr ? 250 : -1);
    if (ready == 0 || (ready < 0 && errno == EINTR))
      continue;
    if (ready < 0 || !(event.revents & POLLIN)) {
      safe_error(error) = "Meadowlark listener failed";
      return false;
    }
    std::unique_ptr<Socket> client(new Socket(accept(listener, nullptr, nullptr)));
    if (client->fd() < 0) {
      if (errno == EINTR || errno == ECONNABORTED)
        continue;
      safe_error(error) = "Cannot accept Meadowlark connection: " +
                          std::string(std::strerror(errno));
      return false;
    }
    serve_connection(std::move(client), warren, activation, hello, sessions);
  }
}

std::string absolute_path(const std::filesystem::path &path,
                          std::string *error) {
  std::error_code failure;
  auto absolute = std::filesystem::absolute(path, failure);
  if (failure) {
    safe_error(error) = "Cannot make path absolute: " + path.string() + ": " +
                        failure.message();
    return "";
  }
  return absolute.string();
}

bool executable_file(const std::filesystem::path &path) {
  struct stat status;
  return stat(path.c_str(), &status) == 0 && S_ISREG(status.st_mode) &&
         access(path.c_str(), X_OK) == 0;
}

std::string shell_quote(const std::string &path) {
  std::string quoted = "'";
  for (char ch : path) {
    if (ch == '\'')
      quoted += "'\\''";
    else
      quoted += ch;
  }
  return quoted + "'";
}

} // namespace

std::string server_path(const std::string &executable, std::string *error) {
  if (executable.find('/') != std::string::npos) {
    if (executable_file(executable))
      return absolute_path(executable, error);
    safe_error(error) = "Not an executable file: " + executable;
    return "";
  }
  if (const char *path = std::getenv("PATH")) {
    std::string_view entries(path);
    for (;;) {
      size_t end = entries.find(':');
      auto candidate = std::filesystem::path(entries.substr(0, end)) / executable;
      if (executable_file(candidate))
        return absolute_path(candidate, error);
      if (end == std::string_view::npos)
        break;
      entries.remove_prefix(end + 1);
    }
  }
  safe_error(error) = "Cannot find executable on PATH: " + executable;
  return "";
}

bool config(const std::string &meadow, const std::string &executable,
            std::string *error) {
  std::string directory = absolute_path(meadow.empty() ? DEFAULT_MEADOW : meadow,
                                        error);
  if (directory.empty())
    return false;
  std::string path = executable.empty() ? server_path("meadowlark", error)
                                        : absolute_path(executable, error);
  if (path.empty())
    return false;
  if (!executable_file(path)) {
    safe_error(error) = "Not an executable file: " + path;
    return false;
  }
  auto wlock = WLock::obtain(directory, error);
  if (wlock == nullptr)
    return false;
  if (wlock->name() == "simple") {
    safe_error(error) = "Cannot configure a server for a legacy Simple burrow";
    return false;
  }
  std::string dna;
  std::map<std::string, std::string> parameters;
  if (!read_dna(wlock->working(), &dna, error) ||
      !cook(dna, &parameters, error))
    return false;
  const std::map<std::string, std::string> settings = {
      {"active", "yes"},
      {"command", shell_quote(path) + " --meadow " + shell_quote(directory) +
                      " --server"}};
  parameters["server"] = freeze(settings);
  std::string updated = freeze(parameters);
  // read_dna joins lines with spaces. Validate the same representation here.
  std::string readback = updated;
  std::replace(readback.begin(), readback.end(), '\n', ' ');
  std::map<std::string, std::string> parsed, parsed_settings;
  if (!cook(readback, &parsed) || !parsed.contains("server") ||
      !cook(parsed.at("server"), &parsed_settings) || parsed_settings != settings) {
    safe_error(error) = "Cannot represent the server command in Meadowlark DNA";
    return false;
  }
  parameters.erase("server");
  parsed.erase("server");
  if (freeze(parameters) != freeze(parsed)) {
    safe_error(error) = "Cannot preserve existing Meadowlark DNA settings";
    return false;
  }
  return write_dna(wlock->working(), updated, error);
}

bool server(const std::string &burrow, std::string *error) {
  // Keep the lock from taking a descriptor that log redirection replaces.
  if (!ensure_output_stream(STDOUT_FILENO, error) ||
      !ensure_output_stream(STDERR_FILENO, error))
    return false;
  std::string lock_error;
  bool busy;
  auto wlock = WLock::obtain(burrow, &lock_error, &busy);
  if (wlock == nullptr) {
    if (busy)
      return true;
    safe_set(error) = lock_error;
    return false;
  }
  auto working = wlock->working();
  if (!redirect_log(working->make_name("meadowlark.log"), error))
    return false;
  const std::time_t started_at = std::time(nullptr);
  std::tm utc{};
  if (gmtime_r(&started_at, &utc) != nullptr)
    std::cerr << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ") << ' ';
  std::cerr << "Meadowlark server starting\n";
  std::string url_file = working->make_name("meadowlark.url");
  if (unlink(url_file.c_str()) != 0 && errno != ENOENT) {
    safe_error(error) = "Cannot remove stale Meadowlark URL: " + url_file;
    return false;
  }
  Descriptor listener(socket(AF_INET, SOCK_STREAM, 0));
  std::string url;
  if (!listen_local(listener.get(), &url, error) ||
      !publish_url(*working, url_file, url, error))
    return false;
  std::cerr << "Meadowlark server listening at " << url << "\n";
  bool success = false;
  try {
    success = serve(listener.get(), wlock, error);
  } catch (const std::exception &exception) {
    safe_error(error) =
        "Meadowlark server failed: " + std::string(exception.what());
  }
  unlink(url_file.c_str());
  return success;
}

namespace {

#if defined(__linux__) && defined(SYS_pidfd_open) && defined(SYS_pidfd_send_signal)
bool local_owner(const std::string &directory, pid_t *pid, std::string *error) {
  std::string filename = directory + "/burrow.pid";
  Descriptor file(open(filename.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW |
                                             O_NONBLOCK));
  struct stat status;
  if (file.get() < 0 || fstat(file.get(), &status) != 0 ||
      !S_ISREG(status.st_mode)) {
    safe_error(error) = "Cannot read Meadowlark lock owner: " + filename;
    return false;
  }
  char buffer[512];
  ssize_t count;
  do {
    count = read(file.get(), buffer, sizeof(buffer));
  } while (count < 0 && errno == EINTR);
  if (count <= 0 || count == static_cast<ssize_t>(sizeof(buffer))) {
    safe_error(error) = "Invalid Meadowlark lock owner: " + filename;
    return false;
  }
  std::string_view owner(buffer, count);
  if (owner.ends_with('\n'))
    owner.remove_suffix(1);
  size_t slash = owner.find('/');
  if (slash == std::string_view::npos) {
    safe_error(error) = "Invalid Meadowlark lock owner: " + filename;
    return false;
  }
  char hostname[256] = {};
  if (gethostname(hostname, sizeof(hostname) - 1) != 0 ||
      owner.substr(0, slash) != hostname) {
    safe_error(error) = "Meadowlark lock owner is not local: " +
                        std::string(owner);
    return false;
  }
  auto end = owner.data() + owner.size();
  auto result = std::from_chars(owner.data() + slash + 1, end, *pid);
  if (result.ec != std::errc() || result.ptr != end || *pid <= 0 ||
      *pid == getpid()) {
    safe_error(error) = "Invalid Meadowlark server PID: " + std::string(owner);
    return false;
  }
  return true;
}

bool matches_server_command(pid_t pid, const std::string &command) {
  wordexp_t words{};
  int status = wordexp(command.c_str(), &words, WRDE_NOCMD | WRDE_UNDEF);
  bool matches = status == 0 && words.we_wordc > 0;
  if (matches) {
    std::ifstream process("/proc/" + std::to_string(pid) + "/cmdline",
                          std::ios::binary);
    std::string argument;
    for (size_t i = 0; matches && i < words.we_wordc; i++)
      matches = std::getline(process, argument, '\0') &&
                argument == words.we_wordv[i];
    matches = matches && process.peek() == std::char_traits<char>::eof() &&
              !process.bad();
  }
  if (status == 0 || status == WRDE_NOSPACE)
    wordfree(&words);
  return matches;
}
#endif

bool terminate_server(const std::string &directory, std::string *error) {
#if defined(__linux__) && defined(SYS_pidfd_open) && defined(SYS_pidfd_send_signal)
  pid_t pid;
  if (!local_owner(directory, &pid, error))
    return false;
  // Keep the process identity stable between checking the command and signaling.
  Descriptor process(static_cast<int>(syscall(SYS_pidfd_open, pid, 0U)));
  if (process.get() < 0) {
    if (errno == ESRCH)
      return true;
    safe_error(error) = "Cannot identify Meadowlark server process: " +
                        std::string(std::strerror(errno));
    return false;
  }
  std::string dna;
  std::map<std::string, std::string> parameters, server;
  if (!read_dna(directory + "/" + DNA_NAME, &dna, error) ||
      !cook(dna, &parameters, error))
    return false;
  auto block = parameters.find("server");
  if (block != parameters.end() && !cook(block->second, &server, error))
    return false;
  auto command = server.find("command");
  if (command == server.end() || command->second.empty()) {
    safe_error(error) = "No configured Meadowlark server command for " + directory;
    return false;
  }
  if (!matches_server_command(pid, command->second)) {
    safe_error(error) = "Burrow lock owner does not match the Meadowlark server "
                        "command: PID " + std::to_string(pid);
    return false;
  }
  if (syscall(SYS_pidfd_send_signal, process.get(), SIGTERM, nullptr, 0U) == 0 ||
      errno == ESRCH)
    return true;
  safe_error(error) = "Cannot signal Meadowlark server: " +
                      std::string(std::strerror(errno));
#else
  safe_error(error) =
      "Cannot verify the Meadowlark server process on this platform";
#endif
  return false;
}

} // namespace

bool shutdown(const std::string &burrow, std::string *error) {
  const std::string &directory = burrow.empty() ? DEFAULT_BURROW : burrow;
  std::string lock_error;
  bool busy;
  if (auto wlock = WLock::obtain(directory, &lock_error, &busy)) {
    if (wlock->name() == "simple") {
      safe_error(error) = "Cannot verify shutdown of a legacy Simple burrow";
      return false;
    }
    return true;
  }
  if (!busy) {
    safe_set(error) = lock_error;
    return false;
  }

  std::string stop_error;
  bool stopping = terminate_server(directory, &stop_error);
  for (int retry = 0; retry <= (stopping ? 10 : 0); retry++) {
    if (retry > 0)
      std::this_thread::sleep_for(std::chrono::seconds(1));
    if (WLock::obtain(directory, &lock_error, &busy) != nullptr)
      return true;
    if (!busy) {
      safe_set(error) = lock_error;
      return false;
    }
  }
  if (stopping)
    safe_error(error) = "Meadowlark server did not release the burrow lock "
                        "within 10 seconds: " + directory;
  else
    safe_set(error) = stop_error;
  return false;
}

} // namespace meadowlark
} // namespace cottontail
