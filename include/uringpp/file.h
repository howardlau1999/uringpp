#pragma once

#include <fcntl.h>
#include <utility>

#include "uringpp/awaitable.h"
#include "uringpp/dir.h"
#include "uringpp/error.h"
#include "uringpp/event_loop.h"
#include "uringpp/socket.h"

#include "uringpp/detail/noncopyable.h"

namespace uringpp {

class file : public noncopyable {
protected:
  std::shared_ptr<event_loop> loop_;
  int fd_;

public:
  int fd() const { return fd_; }

  static task<file> open(std::shared_ptr<event_loop> loop, char const *path,
                         int flags, mode_t mode) {
    int fd = co_await loop->openat(AT_FDCWD, path, flags, mode);
    check_nerrno(fd, "failed to open file");
    co_return file(loop, fd);
  }

  static task<file> openat(std::shared_ptr<event_loop> loop, dir const &dir,
                           char const *path, int flags, mode_t mode) {
    int fd = co_await loop->openat(dir.fd(), path, flags, mode);
    check_nerrno(fd, "failed to open file");
    co_return file(loop, fd);
  }

  static task<file> openat2(std::shared_ptr<event_loop> loop, dir const &dir,
                            char const *path, struct open_how *how) {
    int fd = co_await loop->openat2(dir.fd(), path, how);
    check_nerrno(fd, "failed to open file");
    co_return file(loop, fd);
  }

  file(file &&other) noexcept
      : loop_(std::move(other.loop_)), fd_(std::exchange(other.fd_, -1)) {}

  file(std::shared_ptr<event_loop> loop, int fd) : loop_(loop), fd_(fd) {}

  task<void> close() {
    if (fd_ > 0) {
      co_await loop_->close(fd_);
      fd_ = -1;
    }
  }

  ~file() {
    if (fd_ > 0) {
      loop_->close_detach(fd_);
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

  sqe_awaitable read_fixed(void *buf, size_t count, off_t offset,
                           int buf_index) {
    return loop_->read_fixed(fd_, buf, count, offset, buf_index);
  }

  sqe_awaitable write_fixed(void const *buf, size_t count, off_t offset,
                            int buf_index) {
    return loop_->write_fixed(fd_, buf, count, offset, buf_index);
  }

  sqe_awaitable fsync(int flags) { return loop_->fsync(fd_, flags); }

  sqe_awaitable sync_file_range(off_t offset, off_t nbytes,
                                unsigned sync_range_flags) {
    return loop_->sync_file_range(fd_, offset, nbytes, sync_range_flags);
  }

  sqe_awaitable tee(file const &out, size_t count, unsigned int flags) {
    return loop_->tee(fd_, out.fd(), count, flags);
  }

  sqe_awaitable tee(socket const &out, size_t count, unsigned int flags) {
    return loop_->tee(fd_, out.fd(), count, flags);
  }

  sqe_awaitable splice(loff_t off_in, file const &out, loff_t off_out,
                       size_t nbytes, unsigned flags) {
    return loop_->splice(fd_, off_in, out.fd(), off_out, nbytes, flags);
  }

  sqe_awaitable splice(loff_t off_in, socket const &out, loff_t off_out,
                       size_t nbytes, unsigned flags) {
    return loop_->splice(fd_, off_in, out.fd(), off_out, nbytes, flags);
  }
};

} // namespace uringpp