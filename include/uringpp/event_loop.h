#pragma once

#include <bitset>
#include <cassert>
#include <cstdint>
#include <liburing.h>
#include <memory>
#include <new>
#include <stdexcept>
#include <unordered_set>

#include "uringpp/awaitable.h"
#include "uringpp/error.h"
#include "uringpp/task.h"

#include "uringpp/detail/noncopyable.h"

namespace uringpp {

enum class feature {
  SINGLE_MMAP,
  NODROP,
  SUBMIT_STABLE,
  RW_CUR_POS,
  CUR_PERSONALITY,
  FAST_POLL,
  POLL_32BITS,
  SQPOLL_NONFIXED,
  EXT_ARG,
  NATIVE_WORKERS,
  RSRC_TAGS,
};

class event_loop : public noncopyable,
                   public std::enable_shared_from_this<event_loop> {
  struct io_uring ring_;
  unsigned int cqe_count_;
  std::unordered_set<feature> supported_features_;
  std::bitset<IORING_OP_LAST> supported_ops_;
  class probe_ring {
    struct io_uring_probe *probe_;

  public:
    probe_ring(struct io_uring *ring);
    std::bitset<IORING_OP_LAST> supported_ops();
    ~probe_ring();
  };

  void check_feature(uint32_t features, uint32_t test_bit, enum feature efeat);

  void init_supported_features(struct io_uring_params const &params);

  struct io_uring_sqe *get_sqe() {
    auto sqe = ::io_uring_get_sqe(&ring_);
    if (sqe != nullptr) [[likely]] {
      return sqe;
    }
    ::io_uring_cq_advance(&ring_, cqe_count_);
    cqe_count_ = 0;
    ::io_uring_submit(&ring_);
    sqe = ::io_uring_get_sqe(&ring_);
    if (sqe == nullptr) [[unlikely]] {
      throw std::runtime_error("failed to allocate sqe");
    }
    return sqe;
  }

  sqe_awaitable await_sqe(struct io_uring_sqe *sqe, uint8_t flags) {
    ::io_uring_sqe_set_flags(sqe, flags);
    return sqe_awaitable(sqe);
  }

public:
  static std::shared_ptr<event_loop> create(unsigned int entries = 128,
                                            uint32_t flags = 0, int wq_fd = -1);

  event_loop(unsigned int entries = 128, uint32_t flags = 0, int wq_fd = -1);

  template <class T> void block_on(task<T> t) {
    while (!t.h_.done()) {
      poll();
    }
  }

  int process_cqe() {
    io_uring_cqe *cqe;
    unsigned head;

    io_uring_for_each_cqe(&ring_, head, cqe) {
      ++cqe_count_;
      auto awaitable =
          reinterpret_cast<sqe_awaitable *>(::io_uring_cqe_get_data(cqe));
      if (awaitable != nullptr) [[likely]] {
        awaitable->rc_ = cqe->res;
        awaitable->h_.resume();
      }
    }
    int nr_processed = cqe_count_;
    ::io_uring_cq_advance(&ring_, cqe_count_);
    cqe_count_ = 0;
    return nr_processed;
  }

  int poll_no_wait() {
    ::io_uring_submit(&ring_);
    return process_cqe();
  }

  void poll() {
    ::io_uring_submit_and_wait(&ring_, 1);
    process_cqe();
  }

  sqe_awaitable openat(int dfd, const char *path, int flags, mode_t mode,
                       uint8_t sqe_flags = 0) {
    assert(supported_ops_.test(IORING_OP_OPENAT));
    auto *sqe = get_sqe();
    ::io_uring_prep_openat(sqe, dfd, path, flags, mode);
    return await_sqe(sqe, sqe_flags);
  }

  sqe_awaitable openat2(int dfd, const char *path, struct open_how *how,
                        uint8_t sqe_flags = 0) {
    assert(supported_ops_.test(IORING_OP_OPENAT2));
    auto *sqe = get_sqe();
    ::io_uring_prep_openat2(sqe, dfd, path, how);
    return await_sqe(sqe, sqe_flags);
  }

  sqe_awaitable readv(int fd, const iovec *iovecs, unsigned nr_vecs,
                      off_t offset = 0, uint8_t sqe_flags = 0) {
    assert(supported_ops_.test(IORING_OP_READV));
    auto *sqe = get_sqe();
    ::io_uring_prep_readv(sqe, fd, iovecs, nr_vecs, offset);
    return await_sqe(sqe, sqe_flags);
  }

  sqe_awaitable writev(int fd, const iovec *iovecs, unsigned nr_vecs,
                       off_t offset = 0, uint8_t sqe_flags = 0) {
    assert(supported_ops_.test(IORING_OP_WRITEV));
    auto *sqe = get_sqe();
    ::io_uring_prep_writev(sqe, fd, iovecs, nr_vecs, offset);
    return await_sqe(sqe, sqe_flags);
  }

  sqe_awaitable read(int fd, void *buf, unsigned nbytes, off_t offset = 0,
                     uint8_t sqe_flags = 0) {
    assert(supported_ops_.test(IORING_OP_READ));
    auto *sqe = get_sqe();
    ::io_uring_prep_read(sqe, fd, buf, nbytes, offset);
    return await_sqe(sqe, sqe_flags);
  }

  sqe_awaitable write(int fd, const void *buf, unsigned nbytes,
                      off_t offset = 0, uint8_t sqe_flags = 0) {
    assert(supported_ops_.test(IORING_OP_WRITE));
    auto *sqe = get_sqe();
    ::io_uring_prep_write(sqe, fd, buf, nbytes, offset);
    return await_sqe(sqe, sqe_flags);
  }

  sqe_awaitable read_fixed(int fd, void *buf, unsigned nbytes, off_t offset,
                           int buf_index, uint8_t sqe_flags = 0) {
    assert(supported_ops_.test(IORING_OP_READ_FIXED));
    auto *sqe = get_sqe();
    ::io_uring_prep_read_fixed(sqe, fd, buf, nbytes, offset, buf_index);
    return await_sqe(sqe, sqe_flags);
  }

  sqe_awaitable write_fixed(int fd, const void *buf, unsigned nbytes,
                            off_t offset, int buf_index,
                            uint8_t sqe_flags = 0) {
    assert(supported_ops_.test(IORING_OP_WRITE_FIXED));
    auto *sqe = get_sqe();
    ::io_uring_prep_write_fixed(sqe, fd, buf, nbytes, offset, buf_index);
    return await_sqe(sqe, sqe_flags);
  }

  sqe_awaitable fsync(int fd, unsigned fsync_flags, uint8_t sqe_flags = 0) {
    assert(supported_ops_.test(IORING_OP_FSYNC));
    auto *sqe = get_sqe();
    ::io_uring_prep_fsync(sqe, fd, fsync_flags);
    return await_sqe(sqe, sqe_flags);
  }

  sqe_awaitable sync_file_range(int fd, off64_t offset, off64_t nbytes,
                                unsigned sync_range_flags,
                                uint8_t sqe_flags = 0) {
    assert(supported_ops_.test(IORING_OP_SYNC_FILE_RANGE));
    auto *sqe = get_sqe();
    ::io_uring_prep_rw(IORING_OP_SYNC_FILE_RANGE, sqe, fd, nullptr, nbytes,
                       offset);
    sqe->sync_range_flags = sync_range_flags;
    return await_sqe(sqe, sqe_flags);
  }

  sqe_awaitable recvmsg(int sockfd, msghdr *msg, uint32_t flags,
                        uint8_t sqe_flags = 0) {
    assert(supported_ops_.test(IORING_OP_RECVMSG));
    auto *sqe = get_sqe();
    ::io_uring_prep_recvmsg(sqe, sockfd, msg, flags);
    return await_sqe(sqe, sqe_flags);
  }

  sqe_awaitable sendmsg(int sockfd, const msghdr *msg, uint32_t flags,
                        uint8_t sqe_flags = 0) {
    assert(supported_ops_.test(IORING_OP_SENDMSG));
    auto *sqe = get_sqe();
    ::io_uring_prep_sendmsg(sqe, sockfd, msg, flags);
    return await_sqe(sqe, sqe_flags);
  }

  sqe_awaitable recv(int sockfd, void *buf, unsigned nbytes, uint32_t flags,
                     uint8_t sqe_flags = 0) {
    assert(supported_ops_.test(IORING_OP_RECV));
    auto *sqe = get_sqe();
    ::io_uring_prep_recv(sqe, sockfd, buf, nbytes, flags);
    return await_sqe(sqe, sqe_flags);
  }

  sqe_awaitable send(int sockfd, const void *buf, unsigned nbytes,
                     uint32_t flags, uint8_t sqe_flags = 0) {
    assert(supported_ops_.test(IORING_OP_SEND));
    auto *sqe = get_sqe();
    ::io_uring_prep_send(sqe, sockfd, buf, nbytes, flags);
    return await_sqe(sqe, sqe_flags);
  }

  sqe_awaitable poll_add(int fd, short poll_mask, uint8_t sqe_flags = 0) {
    assert(supported_ops_.test(IORING_OP_POLL_ADD));
    auto *sqe = get_sqe();
    ::io_uring_prep_poll_add(sqe, fd, poll_mask);
    return await_sqe(sqe, sqe_flags);
  }

  sqe_awaitable nop(uint8_t sqe_flags = 0) {
    assert(supported_ops_.test(IORING_OP_NOP));
    auto *sqe = get_sqe();
    ::io_uring_prep_nop(sqe);
    return await_sqe(sqe, sqe_flags);
  }

  sqe_awaitable accept(int fd, sockaddr *addr, socklen_t *addrlen,
                       int flags = 0, uint8_t sqe_flags = 0) {
    assert(supported_ops_.test(IORING_OP_ACCEPT));
    auto *sqe = get_sqe();
    ::io_uring_prep_accept(sqe, fd, addr, addrlen, flags);
    return await_sqe(sqe, sqe_flags);
  }

  sqe_awaitable connect(int fd, sockaddr *addr, socklen_t addrlen,
                        uint8_t sqe_flags = 0) {
    assert(supported_ops_.test(IORING_OP_CONNECT));
    auto *sqe = get_sqe();
    ::io_uring_prep_connect(sqe, fd, addr, addrlen);
    return await_sqe(sqe, sqe_flags);
  }

  sqe_awaitable timeout(__kernel_timespec *ts, uint8_t sqe_flags = 0) {
    assert(supported_ops_.test(IORING_OP_TIMEOUT));
    auto *sqe = get_sqe();
    ::io_uring_prep_timeout(sqe, ts, 0, 0);
    return await_sqe(sqe, sqe_flags);
  }

  sqe_awaitable close(int fd, uint8_t sqe_flags = 0) {
    assert(supported_ops_.test(IORING_OP_CLOSE));
    auto *sqe = get_sqe();
    ::io_uring_prep_close(sqe, fd);
    return await_sqe(sqe, sqe_flags);
  }

  void close_detach(int fd, uint8_t sqe_flags = 0) {
    assert(supported_ops_.test(IORING_OP_CLOSE));
    auto *sqe = get_sqe();
    ::io_uring_prep_close(sqe, fd);
    ::io_uring_sqe_set_flags(sqe, sqe_flags);
    ::io_uring_sqe_set_data(sqe, nullptr);
  }

  sqe_awaitable statx(int dfd, const char *path, int flags, unsigned mask,
                      struct statx *statxbuf, uint8_t sqe_flags = 0) {
    assert(supported_ops_.test(IORING_OP_STATX));
    auto *sqe = get_sqe();
    ::io_uring_prep_statx(sqe, dfd, path, flags, mask, statxbuf);
    return await_sqe(sqe, sqe_flags);
  }

  sqe_awaitable splice(int fd_in, loff_t off_in, int fd_out, loff_t off_out,
                       size_t nbytes, unsigned flags, uint8_t sqe_flags = 0) {
    assert(supported_ops_.test(IORING_OP_SPLICE));
    auto *sqe = get_sqe();
    ::io_uring_prep_splice(sqe, fd_in, off_in, fd_out, off_out, nbytes, flags);
    return await_sqe(sqe, sqe_flags);
  }

  sqe_awaitable tee(int fd_in, int fd_out, size_t nbytes, unsigned flags,
                    uint8_t sqe_flags = 0) {
    assert(supported_ops_.test(IORING_OP_TEE));
    auto *sqe = get_sqe();
    ::io_uring_prep_tee(sqe, fd_in, fd_out, nbytes, flags);
    return await_sqe(sqe, sqe_flags);
  }

  sqe_awaitable shutdown(int fd, int how, uint8_t sqe_flags = 0) {
    assert(supported_ops_.test(IORING_OP_SHUTDOWN));
    auto *sqe = get_sqe();
    ::io_uring_prep_shutdown(sqe, fd, how);
    return await_sqe(sqe, sqe_flags);
  }

  sqe_awaitable renameat(int olddfd, const char *oldpath, int newdfd,
                         const char *newpath, unsigned flags,
                         uint8_t sqe_flags = 0) {
    assert(supported_ops_.test(IORING_OP_RENAMEAT));
    auto *sqe = get_sqe();
    ::io_uring_prep_renameat(sqe, olddfd, oldpath, newdfd, newpath, flags);
    return await_sqe(sqe, sqe_flags);
  }

  sqe_awaitable mkdirat(int dirfd, const char *pathname, mode_t mode,
                        uint8_t sqe_flags = 0) {
    assert(supported_ops_.test(IORING_OP_MKDIRAT));
    auto *sqe = get_sqe();
    ::io_uring_prep_mkdirat(sqe, dirfd, pathname, mode);
    return await_sqe(sqe, sqe_flags);
  }

  sqe_awaitable symlinkat(const char *target, int newdirfd,
                          const char *linkpath, uint8_t sqe_flags = 0) {
    assert(supported_ops_.test(IORING_OP_SYMLINKAT));
    auto *sqe = get_sqe();
    ::io_uring_prep_symlinkat(sqe, target, newdirfd, linkpath);
    return await_sqe(sqe, sqe_flags);
  }

  sqe_awaitable linkat(int olddirfd, const char *oldpath, int newdirfd,
                       const char *newpath, int flags, uint8_t sqe_flags = 0) {
    assert(supported_ops_.test(IORING_OP_LINKAT));
    auto *sqe = get_sqe();
    ::io_uring_prep_linkat(sqe, olddirfd, oldpath, newdirfd, newpath, flags);
    return await_sqe(sqe, sqe_flags);
  }

  sqe_awaitable unlinkat(int dfd, const char *path, unsigned flags,
                         uint8_t sqe_flags = 0) {
    assert(supported_ops_.test(IORING_OP_UNLINKAT));
    auto *sqe = get_sqe();
    ::io_uring_prep_unlinkat(sqe, dfd, path, flags);
    return await_sqe(sqe, sqe_flags);
  }

  void update_files(unsigned off, int *fds, size_t nfds) {
    check_nerrno(::io_uring_register_files_update(&ring_, off, fds, nfds),
                 "failed to update files");
  }

  void register_files(int const *fds, size_t nfds) {
    check_nerrno(::io_uring_register_files(&ring_, fds, nfds),
                 "failed to register files");
  }

  int unregister_files() { return ::io_uring_unregister_files(&ring_); }

  void register_buffers(struct iovec const *iovecs, unsigned nr_iovecs) {
    check_nerrno(::io_uring_register_buffers(&ring_, iovecs, nr_iovecs),
                 "failed to register buffers");
  }

  int unregister_buffers() noexcept {
    return ::io_uring_unregister_buffers(&ring_);
  }

  ~event_loop();
};

} // namespace uringpp

/**
 * \example helloworld.cc
 * Quick example of opening sockets and files and doing some I/O on them.
 *
 */