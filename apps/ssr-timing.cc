#include <cctype>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "src/nlohmann.h"

namespace {

constexpr long long SLOW_QUERY_MS = 30000;

struct TimingQuery {
  std::string tag;
  std::string query;
  bool baseline_seen = false;
  bool have_docno = false;
  std::string docno;
};

struct Pass {
  std::string label;
  bool optimizer;
};

void usage(const std::string &program_name) {
  std::cerr << "usage: " << program_name << " port timing.queries [seconds]\n";
}

std::string timestamp() {
  std::time_t now = std::time(nullptr);
  std::tm tm;
  gmtime_r(&now, &tm);
  char buffer[sizeof("YYYY-MM-DDTHH:MM:SSZ")];
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tm);
  return buffer;
}

std::string trim_left(std::string text) {
  size_t begin = 0;
  while (begin < text.size() &&
         std::isspace(static_cast<unsigned char>(text[begin])))
    begin++;
  return text.substr(begin);
}

bool parse_query_line(const std::string &line, std::string *tag,
                      std::string *query) {
  size_t tag_begin = 0;
  while (tag_begin < line.size() &&
         std::isspace(static_cast<unsigned char>(line[tag_begin])))
    tag_begin++;
  if (tag_begin == line.size())
    return false;
  size_t tag_end = tag_begin;
  while (tag_end < line.size() &&
         !std::isspace(static_cast<unsigned char>(line[tag_end])))
    tag_end++;
  *tag = line.substr(tag_begin, tag_end - tag_begin);
  *query = trim_left(line.substr(tag_end));
  return !tag->empty() && !query->empty();
}

int connect_local(uint16_t port) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    return -1;
  sockaddr_in address;
  std::memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = htons(port);
  if (connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) !=
      0) {
    close(fd);
    return -1;
  }
  return fd;
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

bool request(int fd, const json &request, json *record,
             const std::string &program_name) {
  if (write_all(fd, request.dump() + "\n") <= 0)
    return false;
  std::string response;
  if (!read_line(fd, &response))
    return false;
  try {
    *record = json::parse(response);
  } catch (json::parse_error &e) {
    std::cerr << program_name << ": bad server response: " << e.what() << "\n";
    return false;
  }
  return true;
}

bool set_optimizer(int fd, bool enabled, const std::string &program_name) {
  json request_json;
  request_json["op"] = "set_optimizer";
  request_json["enabled"] = enabled;
  json record;
  if (!request(fd, request_json, &record, program_name))
    return false;
  json response = record.value("response", json::object());
  if (!response.value("ok", false)) {
    std::cerr << program_name << ": "
              << response.value("error", "cannot set optimizer") << "\n";
    return false;
  }
  return true;
}

bool query(int fd, const std::string &query_text, json *record,
           const std::string &program_name) {
  json request_json;
  request_json["op"] = "query";
  request_json["query"] = query_text;
  return request(fd, request_json, record, program_name);
}

bool record_time(const std::string &pass, const std::string &tag,
                 long long elapsed, std::ofstream *log) {
  std::cout << pass << " " << tag << " " << elapsed << "\n";
  std::cout.flush();
  *log << pass << " " << tag << " " << elapsed << "\n";
  log->flush();
  return !log->fail();
}

} // namespace

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  if (argc == 2 && argv[1] == std::string("--help")) {
    usage(program_name);
    return 0;
  }
  if (argc != 3 && argc != 4) {
    usage(program_name);
    return 1;
  }
  uint16_t port = static_cast<uint16_t>(std::strtoul(argv[1], nullptr, 10));
  long long slow_query_ms = SLOW_QUERY_MS;
  if (argc == 4)
    slow_query_ms = 1000 * std::strtoll(argv[3], nullptr, 10);
  std::ifstream query_file(argv[2]);
  if (query_file.fail()) {
    std::cerr << program_name << ": cannot open " << argv[2] << "\n";
    return 1;
  }
  std::vector<TimingQuery> queries;
  std::string line;
  while (std::getline(query_file, line)) {
    TimingQuery query;
    if (parse_query_line(line, &query.tag, &query.query))
      queries.push_back(query);
  }
  std::ofstream log("timing.log", std::ios::app);
  if (log.fail()) {
    std::cerr << program_name << ": cannot open timing.log\n";
    return 1;
  }
  log << timestamp() << "\n";
  log.flush();

  int fd = connect_local(port);
  if (fd < 0) {
    std::cerr << program_name << ": cannot connect to 127.0.0.1:" << port
              << "\n";
    return 1;
  }

  std::vector<Pass> passes = {{"c/opt", true}, {"w/not", false},
                              {"w/opt", true}};
  for (auto &timing_query : queries) {
    for (size_t pass = 0; pass < passes.size(); pass++) {
      if (!set_optimizer(fd, passes[pass].optimizer, program_name)) {
        close(fd);
        return 1;
      }
      json record;
      if (!query(fd, timing_query.query, &record, program_name)) {
        std::cerr << program_name << ": server connection closed\n";
        close(fd);
        return 1;
      }
      json response = record.value("response", json::object());
      if (!response.value("ok", false)) {
        std::cerr << program_name << ": " << timing_query.tag << ": "
                  << response.value("error", "unknown error") << "\n";
        close(fd);
        return 1;
      }
      bool have_docno = response.contains("docno");
      std::string docno = have_docno ? response["docno"].get<std::string>() : "";
      if (pass == 0) {
        timing_query.baseline_seen = true;
        timing_query.have_docno = have_docno;
        timing_query.docno = docno;
      } else if (timing_query.baseline_seen &&
                 (timing_query.have_docno != have_docno ||
                  timing_query.docno != docno)) {
        std::cerr << program_name << ": " << timing_query.tag << ": docno "
                  << "mismatch c/opt=" << timing_query.docno << " "
                  << passes[pass].label << "=" << docno << "\n";
      }
      long long elapsed = record.value("time", -1LL);
      if (elapsed < 0) {
        std::cerr << program_name << ": " << timing_query.tag
                  << ": missing time\n";
        close(fd);
        return 1;
      }
      if (!record_time(passes[pass].label, timing_query.tag, elapsed, &log)) {
        std::cerr << program_name << ": write failure in timing.log\n";
        close(fd);
        return 1;
      }
      if (elapsed > slow_query_ms)
        break;
    }
  }

  if (!set_optimizer(fd, false, program_name)) {
    close(fd);
    return 1;
  }

  close(fd);
  return 0;
}
