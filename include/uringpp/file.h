#pragma once

#include <fcntl.h>
#include <utility>

#include "uringpp/awaitable.h"
#include "uringpp/dir.h"
#include "uringpp/error.h"
#include "uringpp/event_loop.h"
#include "uringpp/pipe.h"
#include "uringpp/socket.h"

#include "uringpp/detail/noncopyable.h"

namespace uringpp {

/**
 * @brief Represents a file.
 *
 */
class file : public noncopyable {
protected:
  std::shared_ptr<event_loop> loop_;
  int fd_;

public:
  /**
   * @brief Get the file descriptor of the file.
   *
   * @return int The file descriptor of the file.
   */
  int fd() const { return fd_; }

  /**
   * @brief Opens a file in the current working directory.
   *
   * @param loop The event loop.
   * @param path The path to the file.
   * @param flags The flags to use when opening the file.
   * @param mode The mode to use when opening the file.
   * @return task<file> The file object.
   */
  static task<file> open(std::shared_ptr<event_loop> loop, char const *path,
                         int flags, mode_t mode) {
    int fd = co_await loop->openat(AT_FDCWD, path, flags, mode);
    check_nerrno(fd, "failed to open file");
    co_return file(loop, fd);
  }

  /**
   * @brief Open a file in the given directory.
   *
   * @param loop The event loop.
   * @param dir The directory to open the file in.
   * @param path The path to the file.
   * @param flags The flags to use when opening the file.
   * @param mode The mode to use when opening the file.
   * @return task<file> The file object.
   */
  static task<file> openat(std::shared_ptr<event_loop> loop, dir const &dir,
                           char const *path, int flags, mode_t mode) {
    int fd = co_await loop->openat(dir.fd(), path, flags, mode);
    check_nerrno(fd, "failed to open file");
    co_return file(loop, fd);
  }

  /**
   * @brief Open a file in the given directory.
   *
   * @param loop The event loop.
   * @param dir The directory to open the file in.
   * @param path The path to the file.
   * @param how The open_how struct to use when opening the file.
   * @return task<file>
   */
  static task<file> openat2(std::shared_ptr<event_loop> loop, dir const &dir,
                            char const *path, struct open_how *how) {
    int fd = co_await loop->openat2(dir.fd(), path, how);
    check_nerrno(fd, "failed to open file");
    co_return file(loop, fd);
  }

  /**
   * @brief Move construct a new file object
   *
   * @param other The file object to move from.
   */
  file(file &&other) noexcept
      : loop_(std::move(other.loop_)), fd_(std::exchange(other.fd_, -1)) {}

  /**
   * @brief Construct a new file object using the given event loop and file
   * descriptor. The fd will be closed when the file object is destroyed.
   * @param loop The event loop.
   * @param fd The file descriptor.
   */
  file(std::shared_ptr<event_loop> loop, int fd) : loop_(loop), fd_(fd) {}

  /**
   * @brief Close the file.
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
   * @brief Destroy the file object. If the fd is still open, it will be closed.
   *
   */
  ~file() {
    if (fd_ > 0) {
      loop_->close_detach(fd_);
    }
  }

  /**
   * @brief Read data from the file.
   *
   * @param buf The buffer to read into.
   * @param count The number of bytes to read.
   * @param offset The offset to start reading from.
   * @return sqe_awaitable
   */
  sqe_awaitable read(void *buf, size_t count, off_t offset = 0) {
    return loop_->read(fd_, buf, count, offset);
  }

  /**
   * @brief Write data to the file.
   *
   * @param buf The buffer to write from.
   * @param count The number of bytes to write.
   * @param offset The offset to start writing from.
   * @return sqe_awaitable
   */
  sqe_awaitable write(void const *buf, size_t count, off_t offset = 0) {
    return loop_->write(fd_, buf, count, offset);
  }

  /**
   * @brief Vectorized read from the file.
   *
   * @param iov The iovec to read into.
   * @param iovcnt The number of iovecs to read.
   * @param offset The offset to start reading from.
   * @return sqe_awaitable
   */
  sqe_awaitable readv(struct iovec const *iov, int iovcnt, off_t offset = 0) {
    return loop_->readv(fd_, iov, iovcnt, offset);
  }

  /**
   * @brief Vectorized write to the file.
   *
   * @param iov The iovec to write from.
   * @param iovcnt The number of iovecs to write.
   * @param offset The offset to start writing from.
   * @return sqe_awaitable
   */
  sqe_awaitable writev(struct iovec const *iov, int iovcnt, off_t offset = 0) {
    return loop_->writev(fd_, iov, iovcnt, offset);
  }

  /**
   * @brief Read data to a preregistered buffer from the file.
   *
   * @param buf The preregistered buffer to read into.
   * @param count The number of bytes to read.
   * @param offset The offset to start reading from.
   * @param buf_index The index of the preregistered buffer to read into.
   * @return sqe_awaitable
   */
  sqe_awaitable read_fixed(void *buf, size_t count, off_t offset,
                           int buf_index) {
    return loop_->read_fixed(fd_, buf, count, offset, buf_index);
  }

  /**
   * @brief Write data from a preregistered buffer to the file.
   *
   * @param buf The preregistered buffer to write from.
   * @param count The number of bytes to write.
   * @param offset The offset to start writing from.
   * @param buf_index The index of the preregistered buffer to write from.
   * @return sqe_awaitable
   */
  sqe_awaitable write_fixed(void const *buf, size_t count, off_t offset,
                            int buf_index) {
    return loop_->write_fixed(fd_, buf, count, offset, buf_index);
  }

  /**
   * @brief Asynchronously flush the file.
   *
   * @param flags The flags to use when flushing the file.
   * @return sqe_awaitable
   */
  sqe_awaitable fsync(int flags) { return loop_->fsync(fd_, flags); }

  /**
   * @brief Asynchronously flush a range of the file.
   *
   * @param offset The offset to start flushing from.
   * @param nbytes The number of bytes to flush.
   * @param sync_range_flags The flags to use when flushing the file.
   * @return sqe_awaitable
   */
  sqe_awaitable sync_file_range(off_t offset, off_t nbytes,
                                unsigned sync_range_flags) {
    return loop_->sync_file_range(fd_, offset, nbytes, sync_range_flags);
  }

  /**
   * @brief Tee data from the file to another file.
   *
   * @param out The file to tee data to.
   * @param count The number of bytes to tee.
   * @param flags The flags to use when teeing the data.
   * @return sqe_awaitable
   */
  sqe_awaitable tee(file const &out, size_t count, unsigned int flags) {
    return loop_->tee(fd_, out.fd(), count, flags);
  }

  /**
   * @brief Tee data from the file to a socket.
   *
   * @param out The socket to tee data to.
   * @param count The number of bytes to tee.
   * @param flags The flags to use when teeing the data.
   * @return sqe_awaitable
   */
  sqe_awaitable tee(socket const &out, size_t count, unsigned int flags) {
    return loop_->tee(fd_, out.fd(), count, flags);
  }

  /**
   * @brief Splice data from the file to a pipe.
   *
   * @param off_in The offset to start splicing from.
   * @param nbytes The number of bytes to splice.
   * @param out The pipe to splice data to.
   * @param flags The flags to use when splicing the data.
   * @return sqe_awaitable
   */
  sqe_awaitable splice_to(loff_t off_in, size_t nbytes, pipe const &out,
                          unsigned flags) {
    return loop_->splice(fd_, off_in, out.writable_fd(), 0, nbytes, flags);
  }

  /**
   * @brief Splice data from a pipe to the file.
   *
   * @param off_out The offset to start splicing to.
   * @param in The pipe to splice data from.
   * @param nbytes The number of bytes to splice.
   * @param flags The flags to use when splicing the data.
   * @return sqe_awaitable
   */
  sqe_awaitable splice_from(pipe const &in, loff_t off_out, size_t nbytes,
                            unsigned flags) {
    return loop_->splice(in.readable_fd(), 0, fd_, off_out, nbytes, flags);
  }
};

} // namespace uringpp