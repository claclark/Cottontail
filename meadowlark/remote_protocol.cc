#include "meadowlark/remote_protocol.h"

#include <cerrno>
#include <charconv>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

namespace cottontail::meadowlark::remote_protocol {

Socket::~Socket() { close(); }

void Socket::close() {
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
}

bool Socket::configure() {
  if (fcntl(fd_, F_SETFD, FD_CLOEXEC) != 0)
    return false;
#ifdef SO_NOSIGPIPE
  int no_sigpipe = 1;
  if (setsockopt(fd_, SOL_SOCKET, SO_NOSIGPIPE, &no_sigpipe,
                 sizeof(no_sigpipe)) != 0)
    return false;
#endif
  return timeout(handshake_timeout);
}

bool Socket::timeout(int seconds) {
  const timeval timeout{seconds, 0};
  return setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) ==
             0 &&
         setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) ==
             0;
}

bool Socket::write_bytes(std::string_view bytes) {
#ifdef MSG_NOSIGNAL
  constexpr int flags = MSG_NOSIGNAL;
#else
  constexpr int flags = 0;
#endif
  while (!bytes.empty()) {
    ssize_t count = send(fd_, bytes.data(), bytes.size(), flags);
    if (count > 0)
      bytes.remove_prefix(count);
    else if (count == 0 || errno != EINTR)
      return false;
  }
  return true;
}

bool Socket::write(const json &message) {
  std::string frame = message.dump();
  frame.push_back('\n');
  return write_bytes(frame);
}

bool Socket::read(json *message) {
  size_t end = pending_.find('\n', offset_);
  if (end == std::string::npos) {
    pending_.erase(0, offset_);
    offset_ = 0;
    do {
      char buffer[8192];
      ssize_t count;
      do {
        count = recv(fd_, buffer, sizeof(buffer), 0);
      } while (count < 0 && errno == EINTR);
      if (count <= 0)
        return false;
      size_t previous_size = pending_.size();
      pending_.append(buffer, count);
      end = pending_.find('\n', previous_size);
    } while (end == std::string::npos);
  }
  *message = json::parse(pending_.data() + offset_, pending_.data() + end,
                         nullptr, false);
  offset_ = end + 1;
  return message->is_object();
}

const std::string *string_field(const json &object, const char *name) {
  auto value = object.find(name);
  return value == object.end() ? nullptr
                               : value->get_ptr<const std::string *>();
}

bool decode_addr(const json &value, addr *address) {
  auto text = value.get_ptr<const std::string *>();
  if (text == nullptr)
    return false;
  const char *end = text->data() + text->size();
  auto result = std::from_chars(text->data(), end, *address);
  return result.ec == std::errc() && result.ptr == end;
}

bool addr_field(const json &object, const char *name, addr *address) {
  auto value = object.find(name);
  return value != object.end() && decode_addr(*value, address);
}

json interval(addr p, addr q, addr v) {
  json result = {{"p", std::to_string(p)}, {"q", std::to_string(q)}};
  if (v != 0)
    result["v"] = std::to_string(v);
  return result;
}

bool greeting(const json &message) {
  auto tag = string_field(message, "protocol");
  auto version = message.find("version");
  return tag != nullptr && *tag == protocol && version != message.end() &&
         version->is_number_integer() && *version >= 0;
}

} // namespace cottontail::meadowlark::remote_protocol
