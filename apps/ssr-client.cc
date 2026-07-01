#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <readline/history.h>
#include <readline/readline.h>

#include "src/nlohmann.h"

namespace {

void usage(const std::string &program_name) {
  std::cerr << "usage: " << program_name << " port\n";
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

bool request(int fd, const json &query, json *record,
             const std::string &program_name) {
  std::string line = query.dump() + "\n";
  if (write_all(fd, line) <= 0)
    return false;
  std::string response;
  if (!read_line(fd, &response))
    return false;
  try {
    *record = json::parse(response);
  } catch (json::parse_error &e) {
    std::cerr << program_name << ": bad server response: " << e.what()
              << "\n";
    return false;
  }
  return true;
}

bool print_record(const json &record, std::string *qid, std::string *docno,
                  const std::string &program_name) {
  json response = record.value("response", json::object());
  std::string op = response.value("op", "");
  if (!response.value("ok", false)) {
    std::cerr << program_name << ": "
              << response.value("error", "unknown error") << "\n";
    return false;
  }
  if (record.contains("time") && op == "query")
    std::cerr << "Ranking took: " << record["time"]
              << " millisecond(s)\n";
  if (response.value("done", false)) {
    if (op == "query" && response.contains("qid"))
      *qid = response["qid"].get<std::string>();
    return true;
  }
  if (response.contains("qid"))
    *qid = response["qid"].get<std::string>();
  if (response.contains("docno"))
    *docno = response["docno"].get<std::string>();
  if (response.contains("snippet")) {
    std::cout << response["snippet"].get<std::string>() << "\n";
    std::cout.flush();
  } else if (response.contains("document")) {
    std::cout << response["document"].get<std::string>() << "\n";
    std::cout.flush();
  }
  return true;
}

} // namespace

int main(int argc, char **argv) {
  std::string program_name = argv[0];
  if (argc != 2 || argv[1] == std::string("--help")) {
    usage(program_name);
    return argc == 2 ? 0 : 1;
  }
  uint16_t port = static_cast<uint16_t>(std::strtoul(argv[1], nullptr, 10));
  int fd = connect_local(port);
  if (fd < 0) {
    std::cerr << program_name << ": cannot connect to 127.0.0.1:" << port
              << "\n";
    return 1;
  }

  std::string qid;
  std::string docno;
  char *line;
  while ((line = readline(">> ")) != nullptr) {
    std::string input = line;
    std::free(line);
    if (!input.empty())
      add_history(input.c_str());
    json query;
    if (input.empty() || input == "@next") {
      if (qid.empty())
        continue;
      query["op"] = "next";
      query["qid"] = qid;
    } else if (input == "@full") {
      if (docno.empty()) {
        std::cerr << program_name << ": no current document\n";
        continue;
      }
      query["op"] = "document";
      query["docno"] = docno;
    } else {
      query["op"] = "query";
      query["query"] = input;
    }
    json record;
    if (!request(fd, query, &record, program_name)) {
      std::cerr << program_name << ": server connection closed\n";
      close(fd);
      return 1;
    }
    print_record(record, &qid, &docno, program_name);
  }
  close(fd);
  return 0;
}
