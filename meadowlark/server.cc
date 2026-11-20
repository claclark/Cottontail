#include "meadowlark/server.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <exception>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>

#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include "src/core.h"
#include "src/json.h"
#include "src/warren.h"
#include "src/wlock.h"

namespace cottontail {
namespace meadowlark {
namespace {

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

void send_response(int fd, std::string_view status, std::string_view type,
                   const std::string &body) {
  std::ostringstream header;
  header << "HTTP/1.1 " << status << "\r\nContent-Type: " << type
         << "\r\nContent-Length: " << body.size()
         << "\r\nConnection: close\r\nCache-Control: no-store\r\n\r\n";
  std::string response = header.str() + body;
#ifdef MSG_NOSIGNAL
  constexpr int flags = MSG_NOSIGNAL;
#else
  constexpr int flags = 0;
#endif
  size_t offset = 0;
  while (offset < response.size()) {
    ssize_t count =
        send(fd, response.data() + offset, response.size() - offset, flags);
    if (count > 0)
      offset += count;
    else if (count == 0 || errno != EINTR)
      return;
  }
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

std::string inspect(Warren &warren, const std::string &query) {
  ReadEpoch epoch(warren);
  std::string error;
  auto hopper = warren.hopper_from_gcl(query, &error);
  if (hopper == nullptr)
    return error;
  auto txt = warren.txt();
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

void serve_connection(int fd, Warren *warren) {
  const timeval timeout{5, 0};
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#ifdef SO_NOSIGPIPE
  int no_sigpipe = 1;
  setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &no_sigpipe, sizeof(no_sigpipe));
#endif
  char first;
  // A leading NUL selects the reserved ClientWarren protocol.
  if (receive(fd, &first, 1) != 1 || first == '\0')
    return;
  std::string header(1, first);
  constexpr size_t header_limit = 16 * 1024;
  while (header.find("\r\n\r\n") == std::string::npos) {
    char buffer[2048];
    size_t available = std::min(sizeof(buffer), header_limit - header.size());
    if (available == 0) {
      send_response(fd, "431 Request Header Fields Too Large", "text/plain",
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
    send_response(fd, "400 Bad Request", "text/plain", "Expected an HTTP GET.\n");
    return;
  }
  if (target != "/" && !target.starts_with("/?q=")) {
    send_response(fd, "404 Not Found", "text/plain", "Not found.\n");
    return;
  }
  std::string query;
  if (target.starts_with("/?q=") &&
      !decode_query(std::string_view(target).substr(4), &query)) {
    send_response(fd, "400 Bad Request", "text/plain",
                  "Invalid query encoding.\n");
    return;
  }
  send_response(fd, "200 OK", "text/html; charset=utf-8", page(warren, query));
}

struct Activation {
  std::shared_ptr<Warren> warren;
  std::string error;
};

bool serve(int listener, std::shared_ptr<WLock> wlock, std::string *error) {
  using namespace std::chrono_literals;
  auto activation = std::async(std::launch::async, [wlock]() {
    Activation result;
    result.warren = Warren::local(wlock, &result.error);
    return result;
  });
  std::shared_ptr<Warren> warren;
  for (;;) {
    if (activation.valid() &&
        activation.wait_for(0ms) == std::future_status::ready) {
      Activation result = activation.get();
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
    Descriptor client(accept(listener, nullptr, nullptr));
    if (client.get() < 0) {
      if (errno == EINTR || errno == ECONNABORTED)
        continue;
      safe_error(error) = "Cannot accept Meadowlark connection: " +
                          std::string(std::strerror(errno));
      return false;
    }
    serve_connection(client.get(), warren.get());
  }
}

} // namespace

bool server(const std::string &burrow, std::string *error) {
  // Keep the lock from taking a descriptor that log redirection replaces.
  if (!ensure_output_stream(STDOUT_FILENO, error) ||
      !ensure_output_stream(STDERR_FILENO, error))
    return false;
  std::string lock_error;
  auto wlock = WLock::obtain(burrow, &lock_error);
  if (wlock == nullptr) {
    if (lock_error.starts_with("Cottontail burrow locked"))
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

} // namespace meadowlark
} // namespace cottontail
