#include "meadowlark/remote_warren.h"

#include <cerrno>
#include <cstring>
#include <map>
#include <mutex>
#include <utility>

#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>

#include "meadowlark/remote_protocol.h"
#include "src/recipe.h"

namespace cottontail::meadowlark {
namespace {

void report_error(std::string *error, const std::string &message) {
  if (error != nullptr)
    safe_error(error) = message;
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
  explicit Connection(std::unique_ptr<Socket> socket)
      : socket_(std::move(socket)) {}

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

std::shared_ptr<RemoteWarren> RemoteWarren::make(int port, std::string *error) {
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
  json reply;
  if (!socket->write_bytes(std::string_view("\0", 1)) ||
      !socket->write(hello) || !socket->read(&reply) || !greeting(reply) ||
      !socket->timeout(0)) {
    report_error(error, "Meadowlark server greeting failed on port " +
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
  if (tokenizer == nullptr ||
      !name_and_recipe(parameters, "featurizer", &name, &recipe, &detail)) {
    report_error(error, detail);
    return nullptr;
  }
  auto featurizer = Featurizer::make(name, recipe, &detail);
  if (featurizer == nullptr) {
    report_error(error, detail);
    return nullptr;
  }
  return std::shared_ptr<RemoteWarren>(
      new RemoteWarren(port, std::make_shared<Connection>(std::move(socket)),
                       std::move(featurizer), std::move(tokenizer)));
}

RemoteWarren::RemoteWarren(int port, std::shared_ptr<Connection> connection,
                           std::shared_ptr<Featurizer> featurizer,
                           std::shared_ptr<Tokenizer> tokenizer)
    : Warren(nullptr, std::move(featurizer), std::move(tokenizer),
             std::make_shared<RemoteIdx>(connection),
             std::make_shared<RemoteTxt>(connection)),
      port_(port), connection_(std::move(connection)) {
  name_ = "remote";
  appender_ = std::make_shared<RemoteAppender>(connection_);
  annotator_ = std::make_shared<RemoteAnnotator>(connection_);
}

std::shared_ptr<Warren> RemoteWarren::clone_(std::string *error) {
  return connection_->check(error) ? make(port_, error) : nullptr;
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

bool RemoteWarren::get_parameter_(const std::string &, std::string *,
                                  std::string *error) {
  report_error(error, "Parameters are unavailable on a RemoteWarren");
  return false;
}
bool RemoteWarren::set_parameter_(const std::string &, const std::string &,
                                  std::string *error) {
  report_error(error, "Parameters are unavailable on a RemoteWarren");
  return false;
}

} // namespace cottontail::meadowlark
