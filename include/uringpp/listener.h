#pragma once

#include <memory>
#include <stdexcept>
#include <sys/socket.h>
#include <utility>

#include "uringpp/event_loop.h"
#include "uringpp/ip_address.h"
#include "uringpp/socket.h"

namespace uringpp {

static inline std::string get_in_addr_string(struct addrinfo *ai) {
  char s[INET6_ADDRSTRLEN];
  inet_ntop(ai->ai_family, get_in_addr(ai->ai_addr), s, sizeof(s));
  return s;
}

/**
 * @brief Listen for incoming connections on a socket.
 *
 */
class listener : public noncopyable {
  std::shared_ptr<event_loop> loop_;
  int fd_;
  listener(std::shared_ptr<event_loop> loop, int fd) : loop_(loop), fd_(fd) {}

public:
  /**
   * @brief Get the file descriptor of the listener.
   *
   * @return int The file descriptor of the listener.
   */
  int fd() const { return fd_; }

  /**
   * @brief Listen for incoming TCP connections on the given address. It will
   * create a socket and bind it to the given address.
   *
   * @param loop The event loop to use.
   * @param hostname The hostname to listen on.
   * @param port The port to listen on.
   * @return listener The listener object
   */
  static listener listen(std::shared_ptr<event_loop> loop,
                         std::string const &hostname, std::string const &port) {
    struct addrinfo hints, *servinfo, *p;
    ::bzero(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (auto rc = getaddrinfo(hostname.empty() ? nullptr : hostname.c_str(),
                              port.c_str(), &hints, &servinfo);
        rc != 0) {
      throw_with("getaddrinfo: %s", gai_strerror(rc));
    }
    for (p = servinfo; p != nullptr; p = p->ai_next) {
      try {
        int fd = ::socket(p->ai_family, p->ai_flags, p->ai_protocol);
        check_errno(fd, "failed to create socket");
        {
          int32_t yes = 1;
          check_rc(
              ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)),
              "failed to set reuse address");
        }
        auto const &ip = get_in_addr_string(p);
        check_errno(::bind(fd, p->ai_addr, p->ai_addrlen), "failed to bind");
        check_errno(::listen(fd, 128), "failed to listen");
        URINGPP_LOG_DEBUG("binding %s:%s", ip.c_str(), port.c_str());
        ::freeaddrinfo(servinfo);
        return listener(loop, fd);
      } catch (std::runtime_error &e) {
        URINGPP_LOG_ERROR("%s", e.what());
        continue;
      }
      break;
    }
    throw std::runtime_error("try all addresses, failed to listen");
  }

  /**
   * @brief Move construct a new listener object
   *
   * @param other
   */
  listener(listener &&other) noexcept
      : loop_(std::move(other.loop_)), fd_(std::exchange(other.fd_, -1)) {}

  /**
   * @brief Accept an incoming connection.
   *
   * @return task<std::pair<ip_address, socket>> A pair of the remote address
   * and the accepted socket
   */
  task<std::pair<ip_address, socket>> accept() {
    ip_address addr;
    auto fd = co_await loop_->accept(
        fd_, reinterpret_cast<struct sockaddr *>(&addr.ss_), &addr.len_);
    check_nerrno(fd, "failed to accept connection");
    co_return std::make_pair(addr, socket(loop_, fd));
  }

   /**
   * @brief Accept an incoming connection and attach it with the specified loop.
   *
   * @return task<std::pair<ip_address, socket>> A pair of the remote address
   * and the accepted socket
   */
  task<std::pair<ip_address, socket>> accept(std::shared_ptr<event_loop> loop) {
    ip_address addr;
    auto fd = co_await loop_->accept(
        fd_, reinterpret_cast<struct sockaddr *>(&addr.ss_), &addr.len_);
    check_nerrno(fd, "failed to accept connection");
    co_return std::make_pair(addr, socket(loop, fd));
  }

  /**
   * @brief Close the listener.
   *
   * @return task<void>
   */
  task<void> close() {
    if (fd_ > 0) {
      co_await loop_->close(fd_);
      fd_ = -1;
    }
  }

  /**
   * @brief Destroy the listener object. If the socket is still open, it will be
   * closed.
   *
   */
  ~listener() {
    if (fd_ > 0) {
      loop_->close_detach(fd_);
    }
  }
};

} // namespace uringpp