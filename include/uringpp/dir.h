#pragma once

#include <fcntl.h>
#include <memory>

#include "uringpp/awaitable.h"
#include "uringpp/event_loop.h"

#include "uringpp/detail/noncopyable.h"

namespace uringpp {

class dir : public noncopyable {
protected:
  std::shared_ptr<event_loop> loop_;
  int fd_;

public:
  int fd() const { return fd_; }

  dir(std::shared_ptr<event_loop> loop, int fd) : loop_(loop), fd_(fd) {}

  dir(dir &&other) noexcept
      : loop_(std::move(other.loop_)), fd_(std::exchange(other.fd_, -1)) {}

  ~dir() {
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

  static task<dir> open(std::shared_ptr<event_loop> loop, char const *path,
                        int flags, mode_t mode) {
    int fd = co_await loop->openat(AT_FDCWD, path, flags, mode);
    check_nerrno(fd, "failed to open dir");
    co_return dir(loop, fd);
  }

  static task<dir> openat(std::shared_ptr<event_loop> loop, dir const &dir,
                          char const *path, int flags, mode_t mode) {
    int fd = co_await loop->openat(dir.fd(), path, flags, mode);
    check_nerrno(fd, "failed to open dir");
    co_return uringpp::dir(loop, fd);
  }

  static task<dir> openat2(std::shared_ptr<event_loop> loop, dir const &dir,
                           char const *path, struct open_how *how) {
    int fd = co_await loop->openat2(dir.fd(), path, how);
    check_nerrno(fd, "failed to open dir");
    co_return uringpp::dir(loop, fd);
  }

  sqe_awaitable statx(const char *path, int flags, unsigned mask,
                      struct statx *statxbuf) {
    return loop_->statx(fd_, path, flags, mask, statxbuf);
  }

  sqe_awaitable mkdir(const char *path, mode_t mode) {
    return loop_->mkdirat(fd_, path, mode);
  }

  sqe_awaitable symlink(const char *oldpath, const char *newpath) {
    return loop_->symlinkat(oldpath, fd_, newpath);
  }

  sqe_awaitable symlink(const char *oldpath, dir const &newdir,
                        const char *newpath) {
    return loop_->symlinkat(oldpath, newdir.fd(), newpath);
  }

  sqe_awaitable link(const char *oldpath, const char *newpath, int flags) {
    return loop_->linkat(fd_, oldpath, fd_, newpath, flags);
  }

  sqe_awaitable link(const char *oldpath, dir const &newdir,
                     const char *newpath, int flags) {
    return loop_->linkat(fd_, oldpath, newdir.fd(), newpath, flags);
  }

  sqe_awaitable rename(const char *oldpath, const char *newpath, int flags) {
    return loop_->renameat(fd_, oldpath, fd_, newpath, flags);
  }

  sqe_awaitable rename(const char *oldpath, dir const &newdir,
                       const char *newpath, int flags) {
    return loop_->renameat(fd_, oldpath, newdir.fd(), newpath, flags);
  }

  sqe_awaitable unlink(const char *path, int flags) {
    return loop_->unlinkat(fd_, path, flags);
  }
};

} // namespace uringpp