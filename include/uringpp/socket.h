#pragma once

#include <arpa/inet.h>
#include <cassert>
#include <memory>
#include <netdb.h>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>

#include "uringpp/awaitable.h"
#include "uringpp/error.h"
#include "uringpp/event_loop.h"
#include "uringpp/task.h"

#include "uringpp/detail/debug.h"
#include "uringpp/detail/noncopyable.h"

namespace uringpp {

class file;
class socket : public noncopyable {
  std::shared_ptr<event_loop> loop_;
  int fd_;
  friend class listener;

public:
  int fd() const { return fd_; }

  socket(socket &&other) noexcept
      : loop_(std::move(other.loop_)), fd_(std::exchange(other.fd_, -1)) {
    assert(fd_ != -1);
  }

  socket(std::shared_ptr<event_loop> loop, int domain, int type, int protocol)
      : loop_(loop), fd_(::socket(domain, type, protocol)) {
    check_errno(fd_, "failed to create socket");
  }

  socket(std::shared_ptr<event_loop> loop, int fd) : loop_(loop), fd_(fd) {}

  static task<socket> connect(std::shared_ptr<event_loop> loop,
                              std::string const &hostname,
                              std::string const &port) {
    struct addrinfo hints, *servinfo, *p;
    ::bzero(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (auto rc =
            ::getaddrinfo(hostname.c_str(), port.c_str(), &hints, &servinfo);
        rc != 0) {
      throw_with("failed to getaddrinfo: %s", ::gai_strerror(rc));
    }
    for (p = servinfo; p != nullptr; p = p->ai_next) {
      int fd = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
      if (fd <= 0) {
        continue;
      }
      auto new_socket = socket(loop, fd);
      int rc =
          co_await loop->connect(new_socket.fd(), p->ai_addr, p->ai_addrlen);
      if (rc == 0) {
        ::freeaddrinfo(servinfo);
        co_return new_socket;
      }
    }
    ::freeaddrinfo(servinfo);
    throw std::runtime_error("failed to connect");
  }

  ~socket() {
    if (fd_ > 0) {
      loop_->close_detach(fd_);
    }
  }

  task<void> close() {
    if (fd_ > 0) {
      co_await loop_->close(fd_);
      fd_ = -1;
    }
  }

  sqe_awaitable read(void *buf, size_t count, off_t offset = 0) {
    return loop_->read(fd_, buf, count, offset);
  }

  sqe_awaitable write(void const *buf, size_t count, off_t offset = 0) {
    return loop_->write(fd_, buf, count, offset);
  }

  sqe_awaitable readv(struct iovec const *iov, int iovcnt, off_t offset = 0) {
    return loop_->readv(fd_, iov, iovcnt, offset);
  }

  sqe_awaitable writev(struct iovec const *iov, int iovcnt, off_t offset = 0) {
    return loop_->writev(fd_, iov, iovcnt, offset);
  }

  sqe_awaitable sendmsg(struct msghdr const *msg, int flags = 0) {
    return loop_->sendmsg(fd_, msg, flags);
  }

  sqe_awaitable recvmsg(struct msghdr *msg, int flags = 0) {
    return loop_->recvmsg(fd_, msg, flags);
  }

  sqe_awaitable send(void const *buf, size_t len, int flags = 0) {
    return loop_->send(fd_, buf, len, flags);
  }

  sqe_awaitable recv(void *buf, size_t len, int flags = 0) {
    return loop_->recv(fd_, buf, len, flags);
  }

  sqe_awaitable shutdown(int how) { return loop_->shutdown(fd_, how); }

  sqe_awaitable tee(file const &out, size_t count, unsigned int flags);

  sqe_awaitable tee(socket const &out, size_t count, unsigned int flags) {
    return loop_->tee(fd_, out.fd(), count, flags);
  }

  sqe_awaitable splice(loff_t off_in, file const &out, loff_t off_out,
                       size_t nbytes, unsigned flags);

  sqe_awaitable splice(loff_t off_in, socket const &out, loff_t off_out,
                       size_t nbytes, unsigned flags) {
    return loop_->splice(fd_, off_in, out.fd(), off_out, nbytes, flags);
  }
};

} // namespace uringpp