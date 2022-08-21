#pragma once

#include <fcntl.h>
#include <memory>

#include "uringpp/awaitable.h"
#include "uringpp/event_loop.h"

#include "uringpp/detail/noncopyable.h"

namespace uringpp {

/**
 * @brief An opened directory.
 * 
 */
class dir : public noncopyable {
protected:
  std::shared_ptr<event_loop> loop_;
  int fd_;

public:
  /**
   * @brief Get the file descriptor of the directory.
   * 
   * @return int The file descriptor of the directory.
   */
  int fd() const { return fd_; }

  /**
   * @brief Construct a new dir object from a file descriptor.
   * 
   * @param loop The event loop. 
   * @param fd The file descriptor.
   */
  dir(std::shared_ptr<event_loop> loop, int fd) : loop_(loop), fd_(fd) {}

  /**
   * @brief Move construct a new dir object
   * 
   * @param other 
   */
  dir(dir &&other) noexcept
      : loop_(std::move(other.loop_)), fd_(std::exchange(other.fd_, -1)) {}

  /**
   * @brief Destroy the dir object. If the directory is open, it will be closed.
   * 
   */
  ~dir() {
    if (fd_ > 0) {
      loop_->close_detach(fd_);
    }
  }

  /**
   * @brief Close the directory.
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
   * @brief Open a directory in the current working directory.
   * 
   * @param loop The event loop. 
   * @param path The path to the directory.
   * @param flags The flags to use when opening the directory. 
   * @param mode The mode to use when opening the directory.
   * @return task<dir> 
   */
  static task<dir> open(std::shared_ptr<event_loop> loop, char const *path,
                        int flags, mode_t mode) {
    int fd = co_await loop->openat(AT_FDCWD, path, flags, mode);
    check_nerrno(fd, "failed to open dir");
    co_return dir(loop, fd);
  }

  /**
   * @brief Open a directory in the given directory.
   * 
   * @param loop The event loop.
   * @param dir The directory to open the directory in.
   * @param path The path to the directory.
   * @param flags The flags to use when opening the directory.
   * @param mode The mode to use when opening the directory.
   * @return task<dir> 
   */
  static task<dir> openat(std::shared_ptr<event_loop> loop, dir const &dir,
                          char const *path, int flags, mode_t mode) {
    int fd = co_await loop->openat(dir.fd(), path, flags, mode);
    check_nerrno(fd, "failed to open dir");
    co_return uringpp::dir(loop, fd);
  }

  /**
   * @brief Open a directory in the given directory.
   * 
   * @param loop The event loop.
   * @param dir The directory to open the directory in.
   * @param path The path to the directory.
   * @param how How to open the directory.
   * @return task<dir> 
   */
  static task<dir> openat2(std::shared_ptr<event_loop> loop, dir const &dir,
                           char const *path, struct open_how *how) {
    int fd = co_await loop->openat2(dir.fd(), path, how);
    check_nerrno(fd, "failed to open dir");
    co_return uringpp::dir(loop, fd);
  }

  /**
   * @brief Stat a file/directory in the directory.
   * 
   * @param path The path to the file/directory.
   * @param flags The flags to use when statting the file/directory.
   * @param mask The mask to use when statting the file/directory.
   * @param statxbuf The statxbuf to use when statting the file/directory.
   * @return sqe_awaitable 
   */
  sqe_awaitable statx(const char *path, int flags, unsigned mask,
                      struct statx *statxbuf) {
    return loop_->statx(fd_, path, flags, mask, statxbuf);
  }

  /**
   * @brief Make a directory.
   * 
   * @param path The path to the directory. 
   * @param mode The mode to use when making the directory.
   * @return sqe_awaitable 
   */
  sqe_awaitable mkdir(const char *path, mode_t mode) {
    return loop_->mkdirat(fd_, path, mode);
  }

  /**
   * @brief Symbolic link a file/directory in the directory.
   * 
   * @param oldpath The path to the file/directory to link. 
   * @param newpath The path to the file/directory to link to.
   * @return sqe_awaitable 
   */
  sqe_awaitable symlink(const char *oldpath, const char *newpath) {
    return loop_->symlinkat(oldpath, fd_, newpath);
  }

  /**
   * @brief Symbolic link a file/directory in the directory to another directory.
   * 
   * @param oldpath The path to the file/directory to link. 
   * @param newdir The directory to link to.
   * @param newpath The path to the file/directory to link to. 
   * @return sqe_awaitable 
   */
  sqe_awaitable symlink(const char *oldpath, dir const &newdir,
                        const char *newpath) {
    return loop_->symlinkat(oldpath, newdir.fd(), newpath);
  }

  /**
   * @brief Link a file/directory in the directory.
   * 
   * @param oldpath The path to the file/directory to link.
   * @param newpath The path to the file/directory to link to.
   * @param flags The flags to use when linking the file/directory.
   * @return sqe_awaitable 
   */
  sqe_awaitable link(const char *oldpath, const char *newpath, int flags) {
    return loop_->linkat(fd_, oldpath, fd_, newpath, flags);
  }

  /**
   * @brief Link a file/directory in the directory to another directory.
   * 
   * @param oldpath The path to the file/directory to link. 
   * @param newdir The directory to link to.
   * @param newpath The path to the file/directory to link to.
   * @param flags The flags to use when linking the file/directory.
   * @return sqe_awaitable 
   */
  sqe_awaitable link(const char *oldpath, dir const &newdir,
                     const char *newpath, int flags) {
    return loop_->linkat(fd_, oldpath, newdir.fd(), newpath, flags);
  }

  /**
   * @brief Rename a file/directory in the directory.
   * 
   * @param oldpath The path to the file/directory to rename. 
   * @param newpath The path to the file/directory to rename to.
   * @param flags The flags to use when renaming the file/directory.
   * @return sqe_awaitable 
   */
  sqe_awaitable rename(const char *oldpath, const char *newpath, int flags) {
    return loop_->renameat(fd_, oldpath, fd_, newpath, flags);
  }

  /**
   * @brief Rename a file/directory in the directory to another directory.
   * 
   * @param oldpath The path to the file/directory to rename. 
   * @param newdir The directory to rename to.
   * @param newpath The path to the file/directory to rename to. 
   * @param flags The flags to use when renaming the file/directory.
   * @return sqe_awaitable 
   */
  sqe_awaitable rename(const char *oldpath, dir const &newdir,
                       const char *newpath, int flags) {
    return loop_->renameat(fd_, oldpath, newdir.fd(), newpath, flags);
  }

  /**
   * @brief Unlink a file/directory in the directory.
   * 
   * @param path The path to the file/directory to unlink. 
   * @param flags The flags to use when unlink the file/directory. 
   * @return sqe_awaitable 
   */
  sqe_awaitable unlink(const char *path, int flags) {
    return loop_->unlinkat(fd_, path, flags);
  }
};

} // namespace uringpp