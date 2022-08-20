#pragma once

#include <coroutine>
#include <liburing.h>

namespace uringpp {

class sqe_awaitable {
  friend class event_loop;
  struct io_uring_sqe *sqe_;
  std::coroutine_handle<> h_;
  int rc_;

public:
  sqe_awaitable(struct io_uring_sqe *sqe) : sqe_(sqe) {}
  bool await_ready() noexcept { return false; }
  bool await_suspend(std::coroutine_handle<> h) {
    h_ = h;
    ::io_uring_sqe_set_data(sqe_, this);
    return true;
  }
  int await_resume() { return rc_; }
};

} // namespace uringpp