#ifndef COTTONTAIL_MEADOWLARK_REMOTE_PROTOCOL_H_
#define COTTONTAIL_MEADOWLARK_REMOTE_PROTOCOL_H_

#include <string>
#include <string_view>

#include "src/core.h"
#include "src/nlohmann.h"

namespace cottontail::meadowlark::remote_protocol {

constexpr char protocol[] = "meadowlark";
constexpr int version = 0;
constexpr int handshake_timeout = 5;

class Socket {
public:
  explicit Socket(int fd) : fd_(fd) {}
  ~Socket();
  Socket(const Socket &) = delete;
  Socket &operator=(const Socket &) = delete;

  int fd() const { return fd_; }
  void close();
  bool configure();
  bool timeout(int seconds);
  bool write_bytes(std::string_view bytes);
  bool write(const json &message);
  bool read(json *message);

private:
  int fd_;
  std::string pending_;
  size_t offset_ = 0;
};

const std::string *string_field(const json &object, const char *name);
bool decode_addr(const json &value, addr *address);
bool addr_field(const json &object, const char *name, addr *address);
json interval(addr p, addr q, addr v = 0);
bool greeting(const json &message);

} // namespace cottontail::meadowlark::remote_protocol

#endif // COTTONTAIL_MEADOWLARK_REMOTE_PROTOCOL_H_
