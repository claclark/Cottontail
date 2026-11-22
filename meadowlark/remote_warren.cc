#include "meadowlark/remote_warren.h"

#include <cerrno>
#include <charconv>
#include <chrono>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <thread>
#include <utility>

#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>

#include "meadowlark/remote_protocol.h"
#include "src/dna.h"
#include "src/recipe.h"
#include "src/stemmer.h"

namespace cottontail::meadowlark {
namespace {

void report_error(std::string *error, const std::string &message) {
  if (error != nullptr)
    safe_error(error) = message;
}

int endpoint_port(const std::string &filename) {
  std::ifstream input(filename);
  std::string url, extra;
  if (!(input >> url) || input >> extra)
    return 0;
  constexpr std::string_view prefix = "http://127.0.0.1:";
  std::string_view number(url);
  if (!number.starts_with(prefix) || !number.ends_with('/'))
    return 0;
  number.remove_prefix(prefix.size());
  number.remove_suffix(1);
  int port;
  auto end = number.data() + number.size();
  auto result = std::from_chars(number.data(), end, port);
  if (result.ec != std::errc() || result.ptr != end || port <= 0 ||
      port > UINT16_MAX)
    return 0;
  return port;
}

bool connect_loopback(int fd, int port) {
  int flags = fcntl(fd, F_GETFL);
  if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) != 0)
    return false;
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = htons(static_cast<uint16_t>(port));
  if (connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) !=
      0) {
    if (errno != EINPROGRESS)
      return false;
    pollfd event{fd, POLLOUT, 0};
    int ready = poll(&event, 1, remote_protocol::handshake_timeout * 1000);
    if (ready <= 0) {
      if (ready == 0)
        errno = ETIMEDOUT;
      return false;
    }
    int status;
    socklen_t size = sizeof(status);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &status, &size) != 0)
      return false;
    if (status != 0) {
      errno = status;
      return false;
    }
  }
  return fcntl(fd, F_SETFL, flags) == 0;
}

} // namespace

namespace remote_protocol {

class Connection {
public:
  Connection(std::unique_ptr<Socket> socket, std::string session)
      : socket_(std::move(socket)), session_(std::move(session)) {}

  const std::string &session() const { return session_; }

  bool check(std::string *error) {
    std::lock_guard<std::mutex> lock(mutex_);
    return available(error);
  }

  json request(const char *component, const char *method, json arguments,
               std::string *error = nullptr, const char *field = "response") {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!available(error))
      return {};
    arguments["component"] = component;
    arguments["method"] = method;
    json reply;
    try {
      if (!socket_->write(arguments) || !socket_->read(&reply)) {
        lost(error);
        return {};
      }
    } catch (const json::type_error &) {
      report_error(error, "Meadowlark request contains invalid UTF-8");
      return {};
    }
    if (auto message = string_field(reply, "error")) {
      report_error(error, *message);
      return {};
    }
    auto result = reply.find(field);
    if (result == reply.end()) {
      report_error(error, "Invalid reply from Meadowlark server");
      return {};
    }
    return std::move(*result);
  }

  bool acknowledge(const char *component, const char *method, json arguments,
                   std::string *error = nullptr) {
    return request(component, method, std::move(arguments), error) == true;
  }

  void complete(const std::string &handle) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (socket_->fd() >= 0 && !socket_->write({{"component", "hopper"},
                                               {"method", "complete"},
                                               {"hopper", handle}}))
      lost(nullptr);
  }

private:
  bool available(std::string *error) {
    if (socket_->fd() >= 0)
      return true;
    report_error(error, "Meadowlark server connection lost");
    return false;
  }

  void lost(std::string *error) {
    socket_->close();
    report_error(error, "Meadowlark server connection lost");
  }

  std::mutex mutex_;
  std::unique_ptr<Socket> socket_;
  const std::string session_;
};

} // namespace remote_protocol

namespace {

using namespace remote_protocol;

class RemoteHopper final : public Hopper {
public:
  RemoteHopper(std::shared_ptr<Connection> connection, std::string handle)
      : connection_(std::move(connection)), handle_(std::move(handle)) {}
  ~RemoteHopper() override { connection_->complete(handle_); }

private:
  json seek(const char *method, addr k) {
    return connection_->request(
        "hopper", method, {{"hopper", handle_}, {"k", std::to_string(k)}});
  }

  using Seek = void (Hopper::*)(addr, addr *, addr *, fval *);
  void seek(const char *method, Seek fallback, addr k, addr *p, addr *q,
            fval *v) {
    json result = seek(method, k);
    addr value = 0;
    if (addr_field(result, "p", p) && addr_field(result, "q", q) &&
        (!result.contains("v") || addr_field(result, "v", &value))) {
      *v = addr2fval(value);
    } else {
      (empty_.*fallback)(k, p, q, v);
    }
  }

  void tau_(addr k, addr *p, addr *q, fval *v) final {
    seek("tau", &Hopper::tau, k, p, q, v);
  }
  void rho_(addr k, addr *p, addr *q, fval *v) final {
    seek("rho", &Hopper::rho, k, p, q, v);
  }
  void uat_(addr k, addr *p, addr *q, fval *v) final {
    seek("uat", &Hopper::uat, k, p, q, v);
  }
  void ohr_(addr k, addr *p, addr *q, fval *v) final {
    seek("ohr", &Hopper::ohr, k, p, q, v);
  }
  addr L_(addr k) final {
    addr p;
    return decode_addr(seek("L", k), &p) ? p : empty_.L(k);
  }
  addr R_(addr k) final {
    addr q;
    return decode_addr(seek("R", k), &q) ? q : empty_.R(k);
  }

  std::shared_ptr<Connection> connection_;
  const std::string handle_;
  EmptyHopper empty_;
};

std::unique_ptr<Hopper> make_hopper(std::shared_ptr<Connection> connection,
                                    const char *component, const char *method,
                                    json arguments, std::string *error) {
  json result = connection->request(component, method, std::move(arguments),
                                    error, "hopper");
  auto handle = result.get_ptr<std::string *>();
  return handle == nullptr ? nullptr
                           : std::make_unique<RemoteHopper>(
                                 std::move(connection), std::move(*handle));
}

class RemoteFeaturizer final : public Featurizer {
public:
  explicit RemoteFeaturizer(std::shared_ptr<Connection> connection)
      : connection_(std::move(connection)) {}

private:
  std::string request_text(const char *method, json arguments = {}) {
    json result =
        connection_->request("featurizer", method, std::move(arguments));
    auto text = result.get_ptr<std::string *>();
    return text == nullptr ? std::string() : std::move(*text);
  }

  std::string name_() final { return request_text("name"); }
  std::string recipe_() final { return request_text("recipe"); }
  addr featurize_(const char *key, addr length) final {
    if (key == nullptr || length < 0)
      return null_feature;
    json result = connection_->request("featurizer", "featurize",
                                       {{"label", std::string(key, length)}});
    addr feature;
    return decode_addr(result, &feature) ? feature : null_feature;
  }
  std::string translate_(addr feature) final {
    return request_text("translate", {{"feature", std::to_string(feature)}});
  }

  std::shared_ptr<Connection> connection_;
};

class RemoteIdx final : public Idx {
public:
  explicit RemoteIdx(std::shared_ptr<Connection> connection)
      : connection_(std::move(connection)) {}

private:
  std::string recipe_() final { return ""; }
  std::unique_ptr<Hopper> hopper_(addr feature) final {
    auto hopper = make_hopper(connection_, "idx", "hopper",
                              {{"feature", std::to_string(feature)}}, nullptr);
    if (hopper == nullptr)
      hopper = std::make_unique<EmptyHopper>();
    return hopper;
  }
  addr count_(addr feature) final {
    addr count;
    return decode_addr(
               connection_->request("idx", "count",
                                    {{"feature", std::to_string(feature)}}),
               &count)
               ? count
               : 0;
  }
  addr vocab_() final {
    addr count;
    return decode_addr(connection_->request("idx", "vocab", {}), &count) ? count
                                                                         : 0;
  }
  std::shared_ptr<Connection> connection_;
};

class RemoteTxt final : public Txt {
public:
  explicit RemoteTxt(std::shared_ptr<Connection> connection)
      : connection_(std::move(connection)) {}

private:
  std::string name_() final { return "remote"; }
  std::string recipe_() final { return ""; }
  std::string translate_(addr p, addr q) final {
    json result = connection_->request("txt", "translate", interval(p, q));
    auto text = result.get_ptr<std::string *>();
    return text == nullptr ? std::string() : std::move(*text);
  }
  addr tokens_() final {
    addr count;
    return decode_addr(connection_->request("txt", "tokens", {}), &count)
               ? count
               : 0;
  }
  bool range_(addr *p, addr *q) final {
    json result = connection_->request("txt", "range", {});
    return addr_field(result, "p", p) && addr_field(result, "q", q);
  }

  std::shared_ptr<Connection> connection_;
};

class RemoteAppender final : public Appender {
public:
  explicit RemoteAppender(std::shared_ptr<Connection> connection)
      : connection_(std::move(connection)) {}

private:
  std::string recipe_() final { return ""; }
  bool append_(const std::string &text, addr *p, addr *q,
               std::string *error) final {
    json result =
        connection_->request("appender", "append", {{"text", text}}, error);
    return addr_field(result, "p", p) && addr_field(result, "q", q);
  }
  std::shared_ptr<Connection> connection_;
};

class RemoteAnnotator final : public Annotator {
public:
  explicit RemoteAnnotator(std::shared_ptr<Connection> connection)
      : connection_(std::move(connection)) {}

private:
  std::string recipe_() final { return ""; }
  bool annotate_(addr feature, addr p, addr q, fval v,
                 std::string *error) final {
    json arguments = interval(p, q, fval2addr(v));
    arguments["feature"] = std::to_string(feature);
    return connection_->acknowledge("annotator", "annotate",
                                    std::move(arguments), error);
  }
  bool erase_(addr p, addr q, std::string *error) final {
    return connection_->acknowledge("annotator", "erase", interval(p, q),
                                    error);
  }
  std::shared_ptr<Connection> connection_;
};

} // namespace

std::shared_ptr<RemoteWarren> RemoteWarren::make(const std::string &burrow,
                                                 std::string *error) {
  const std::string &directory = burrow.empty() ? DEFAULT_BURROW : burrow;
  std::string filename = directory + "/meadowlark.url";
  int port = endpoint_port(filename);
  if (port == 0) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    port = endpoint_port(filename);
  }
  if (port == 0) {
    report_error(error, "Cannot find a Meadowlark loopback URL in " + filename);
    return nullptr;
  }
  return make(port, error);
}

std::shared_ptr<RemoteWarren> RemoteWarren::make(int port, std::string *error) {
  return make(port, "", error);
}

std::shared_ptr<RemoteWarren>
RemoteWarren::make(int port, const std::string &clone_of, std::string *error) {
  if (port <= 0 || port > UINT16_MAX) {
    report_error(error, "Invalid Meadowlark server port");
    return nullptr;
  }
  std::unique_ptr<Socket> socket(new Socket(::socket(AF_INET, SOCK_STREAM, 0)));
  if (socket->fd() < 0 || !socket->configure() ||
      !connect_loopback(socket->fd(), port)) {
    report_error(error, "Cannot connect to Meadowlark server on port " +
                            std::to_string(port) + ": " + std::strerror(errno));
    return nullptr;
  }
  json hello = {{"protocol", protocol}, {"version", version}};
  if (!clone_of.empty())
    hello["clone_of"] = clone_of;
  json reply;
  if (!socket->write_bytes(std::string_view("\0", 1)) ||
      !socket->write(hello) || !socket->timeout(0) || !socket->read(&reply)) {
    report_error(error, "Meadowlark server greeting failed on port " +
                            std::to_string(port));
    return nullptr;
  }
  if (auto message = string_field(reply, "error")) {
    report_error(error, *message);
    return nullptr;
  }
  auto session = string_field(reply, "session");
  auto started = reply.find("started");
  if (!greeting(reply) || session == nullptr || session->empty() ||
      started == reply.end() || !started->is_boolean()) {
    report_error(error, "Invalid Meadowlark server greeting on port " +
                            std::to_string(port));
    return nullptr;
  }
  auto dna = string_field(reply, "dna");
  std::map<std::string, std::string> parameters;
  if (dna == nullptr || !cook(*dna, &parameters)) {
    report_error(error, "Meadowlark server returned invalid DNA");
    return nullptr;
  }
  std::string name, recipe, detail;
  if (!name_and_recipe(parameters, "tokenizer", &name, &recipe, &detail)) {
    report_error(error, detail);
    return nullptr;
  }
  auto tokenizer = Tokenizer::make(name, recipe, &detail);
  if (tokenizer == nullptr) {
    report_error(error, detail);
    return nullptr;
  }
  std::map<std::string, std::string> warren_parameters;
  auto settings = parameters.find("parameters");
  if (settings != parameters.end() &&
      !cook(settings->second, &warren_parameters, &detail)) {
    report_error(error, detail);
    return nullptr;
  }
  std::shared_ptr<Stemmer> stemmer;
  auto stemmer_name = warren_parameters.find("stemmer");
  if (stemmer_name != warren_parameters.end() &&
      !stemmer_name->second.empty()) {
    stemmer = Stemmer::make(stemmer_name->second, "", &detail);
    if (stemmer == nullptr) {
      report_error(error, detail);
      return nullptr;
    }
  }
  auto warren = std::shared_ptr<RemoteWarren>(new RemoteWarren(
      port, std::make_shared<Connection>(std::move(socket), *session),
      std::move(tokenizer), std::move(warren_parameters),
      started->get<bool>()));
  if (stemmer != nullptr)
    warren->stemmer_ = std::move(stemmer);
  return warren;
}

RemoteWarren::RemoteWarren(int port, std::shared_ptr<Connection> connection,
                           std::shared_ptr<Tokenizer> tokenizer,
                           std::map<std::string, std::string> parameters,
                           bool started)
    : Warren(nullptr, std::make_shared<RemoteFeaturizer>(connection),
             std::move(tokenizer), std::make_shared<RemoteIdx>(connection),
             std::make_shared<RemoteTxt>(connection), started),
      port_(port), connection_(std::move(connection)),
      parameters_(std::move(parameters)) {
  name_ = "remote";
  auto container = parameters_.find("container");
  if (container != parameters_.end())
    default_container_ = container->second;
  appender_ = std::make_shared<RemoteAppender>(connection_);
  annotator_ = std::make_shared<RemoteAnnotator>(connection_);
}

std::shared_ptr<Warren> RemoteWarren::clone_(std::string *error) {
  return connection_->check(error) ? make(port_, connection_->session(), error)
                                   : nullptr;
}

std::unique_ptr<Hopper> RemoteWarren::hopper_from_gcl_(const std::string &query,
                                                       std::string *error) {
  return make_hopper(connection_, "warren", "hopper_from_gcl",
                     {{"query", query}}, error);
}

void RemoteWarren::start_() { connection_->acknowledge("warren", "start", {}); }
void RemoteWarren::end_() { connection_->acknowledge("warren", "end", {}); }

bool RemoteWarren::transaction_(std::string *error) {
  return connection_->acknowledge("warren", "transaction", {}, error);
}
bool RemoteWarren::ready_(std::string *error) {
  return connection_->acknowledge("warren", "ready", {}, error);
}
void RemoteWarren::commit_() {
  connection_->acknowledge("warren", "commit", {});
}
void RemoteWarren::abort_() { connection_->acknowledge("warren", "abort", {}); }

bool RemoteWarren::get_parameter_(const std::string &key, std::string *value,
                                  std::string *) {
  auto setting = parameters_.find(key);
  if (value != nullptr)
    *value = setting == parameters_.end() ? "" : setting->second;
  return true;
}
bool RemoteWarren::set_parameter_(const std::string &, const std::string &,
                                  std::string *error) {
  report_error(error, "RemoteWarren parameters are read-only");
  return false;
}

} // namespace cottontail::meadowlark
