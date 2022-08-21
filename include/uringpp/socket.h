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
#include "uringpp/pipe.h"
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
  /**
   * @brief Get the file descriptor of the socket.
   *
   * @return int The file descriptor of the socket.
   */
  int fd() const { return fd_; }

  /**
   * @brief Move construct a new socket object
   *
   * @param other
   */
  socket(socket &&other) noexcept
      : loop_(std::move(other.loop_)), fd_(std::exchange(other.fd_, -1)) {
    assert(fd_ != -1);
  }

  /**
   * @brief Construct a new socket object
   *
   * @param loop The event loop.
   * @param domain The domain of the socket.
   * @param type The type of the socket.
   * @param protocol The protocol of the socket.
   */
  socket(std::shared_ptr<event_loop> loop, int domain, int type, int protocol)
      : loop_(loop), fd_(::socket(domain, type, protocol)) {
    check_errno(fd_, "failed to create socket");
  }

  /**
   * @brief Construct a new socket object from a file descriptor.
   *
   * @param loop The event loop.
   * @param fd The file descriptor.
   */
  socket(std::shared_ptr<event_loop> loop, int fd) : loop_(loop), fd_(fd) {
    assert(fd_ > 0);
  }

  /**
   * @brief Connect to a remote host.
   *
   * @param loop The event loop.
   * @param hostname The hostname of the remote host.
   * @param port The port of the remote host.
   * @return task<socket> The socket object.
   */
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

  /**
   * @brief Destroy the socket object. If the socket is still open, it will be
   * closed.
   *
   */
  ~socket() {
    if (fd_ > 0) {
      loop_->close_detach(fd_);
    }
  }

  /**
   * @brief Close the socket.
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
   * @brief Read data from the socket.
   *
   * @param buf The buffer to read into.
   * @param count The number of bytes to read.
   * @return sqe_awaitable
   */
  sqe_awaitable read(void *buf, size_t count) {
    return loop_->read(fd_, buf, count, 0);
  }

  /**
   * @brief Write data to the socket.
   *
   * @param buf The buffer to write from.
   * @param count The number of bytes to write.
   * @return sqe_awaitable
   */
  sqe_awaitable write(void const *buf, size_t count) {
    return loop_->write(fd_, buf, count, 0);
  }

  /**
   * @brief Vectorized read from the socket.
   *
   * @param iov The vector of buffers to read into.
   * @param iovcnt The number of buffers to read.
   * @return sqe_awaitable
   */
  sqe_awaitable readv(struct iovec const *iov, int iovcnt) {
    return loop_->readv(fd_, iov, iovcnt);
  }

  /**
   * @brief Vectorized write data to the socket.
   *
   * @param iov The iovec to write from.
   * @param iovcnt The number of iovecs to write.
   * @return sqe_awaitable
   */
  sqe_awaitable writev(struct iovec const *iov, int iovcnt) {
    return loop_->writev(fd_, iov, iovcnt, 0);
  }

  /**
   * @brief Read data to a preregistered buffer from the socket.
   *
   * @param buf The preregistered buffer to read into.
   * @param count The number of bytes to read.
   * @param buf_index The index of the preregistered buffer to read into.
   * @return sqe_awaitable
   */
  sqe_awaitable read_fixed(void *buf, size_t count, int buf_index) {
    return loop_->read_fixed(fd_, buf, count, 0, buf_index);
  }

  /**
   * @brief Write data from a preregistered buffer to the socket.
   *
   * @param buf The preregistered buffer to write from.
   * @param count The number of bytes to write.
   * @param buf_index The index of the preregistered buffer to write from.
   * @return sqe_awaitable
   */
  sqe_awaitable write_fixed(void const *buf, size_t count, int buf_index) {
    return loop_->write_fixed(fd_, buf, count, 0, buf_index);
  }

  /**
   * @brief Send messages to the socket.
   *
   * @param msg The messages to send.
   * @param flags The flags to use.
   * @return sqe_awaitable
   */
  sqe_awaitable sendmsg(struct msghdr const *msg, int flags = 0) {
    return loop_->sendmsg(fd_, msg, flags);
  }

  /**
   * @brief Receive messages from the socket.
   *
   * @param msg The messages to receive.
   * @param flags The flags to use.
   * @return sqe_awaitable
   */
  sqe_awaitable recvmsg(struct msghdr *msg, int flags = 0) {
    return loop_->recvmsg(fd_, msg, flags);
  }

  /**
   * @brief Send data to the socket.
   *
   * @param buf The buffer to send.
   * @param len The number of bytes to send.
   * @param flags The flags to use.
   * @return sqe_awaitable
   */
  sqe_awaitable send(void const *buf, size_t len, int flags = 0) {
    return loop_->send(fd_, buf, len, flags);
  }

  /**
   * @brief Receive data from the socket.
   *
   * @param buf The buffer to receive into.
   * @param len The number of bytes to receive.
   * @param flags The flags to use.
   * @return sqe_awaitable
   */
  sqe_awaitable recv(void *buf, size_t len, int flags = 0) {
    return loop_->recv(fd_, buf, len, flags);
  }

  /**
   * @brief Shutdown the socket.
   *
   * @param how Which sides of the socket to shutdown.
   * @return sqe_awaitable
   */
  sqe_awaitable shutdown(int how) { return loop_->shutdown(fd_, how); }

  /**
   * @brief Tee the socket to a file.
   *
   * @param out The file to tee to.
   * @param count The number of bytes to tee.
   * @param flags The flags to use.
   * @return sqe_awaitable
   */
  sqe_awaitable tee(file const &out, size_t count, unsigned int flags);

  /**
   * @brief Tee the socket to another socket.
   *
   * @param out The socket to tee to.
   * @param count The number of bytes to tee.
   * @param flags The flags to use.
   * @return sqe_awaitable
   */
  sqe_awaitable tee(socket const &out, size_t count, unsigned int flags) {
    return loop_->tee(fd_, out.fd(), count, flags);
  }

  /**
   * @brief Splice the socket to a pipe.
   *
   * @param out The pipe to splice to.
   * @param off_out The offset to splice to.
   * @param nbytes The number of bytes to splice.
   * @param flags The flags to use.
   * @return sqe_awaitable
   */
  sqe_awaitable splice_to(pipe const &out, size_t nbytes, unsigned flags) {
    return loop_->splice(fd_, 0, out.writable_fd(), 0, nbytes, flags);
  }

  /**
   * @brief Splice from a pipe to the socket.
   *
   * @param in The pipe to splice from.
   * @param nbytes The number of bytes to splice.
   * @param flags The flags to use.
   * @return sqe_awaitable
   */
  sqe_awaitable splice_from(pipe const &in, size_t nbytes, unsigned flags) {
    return loop_->splice(in.readable_fd(), 0, fd_, 0, nbytes, flags);
  }
};

} // namespace uringpp