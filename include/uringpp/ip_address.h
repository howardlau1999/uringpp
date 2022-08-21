#pragma once

#include <arpa/inet.h>
#include <cstdint>
#include <netdb.h>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>

namespace uringpp {

static inline void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

static inline uint16_t get_in_port(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return (((struct sockaddr_in *)sa)->sin_port);
  }

  return (((struct sockaddr_in6 *)sa)->sin6_port);
}

class ip_address {
public:
  struct sockaddr_storage ss_;
  socklen_t len_;
  uint16_t port() {
    return get_in_port(reinterpret_cast<struct sockaddr *>(&ss_));
  }
  std::string ip() {
    char s[INET6_ADDRSTRLEN];
    ::inet_ntop(ss_.ss_family,
                get_in_addr(reinterpret_cast<struct sockaddr *>(&ss_)), s,
                sizeof(s));
    return s;
  }
};

} // namespace uringpp