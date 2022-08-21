#pragma once

#include <cassert>
#include <memory>
#include <unistd.h>

#include "uringpp/error.h"
#include "uringpp/event_loop.h"
namespace uringpp {

class pipe {
  std::shared_ptr<event_loop> loop_;
  int fds_[2];

public:
  /**
   * @brief Construct a new pipe object
   *
   * @param loop The event loop.
   */
  pipe(std::shared_ptr<event_loop> loop) : loop_(loop) {
    check_errno(::pipe(fds_), "failed to create pipe");
    assert(fds_[0] > 0);
    assert(fds_[1] > 0);
  }

  /**
   * @brief Move construct a new pipe object
   *
   * @param other
   */
  pipe(pipe &&other) noexcept
      : loop_(std::move(other.loop_)), fds_{std::exchange(other.fds_[0], -1),
                                            std::exchange(other.fds_[1], -1)} {}

  /**
   * @brief Get the read end of the pipe.
   *
   * @return int The read end of the pipe.
   *
   */
  int readable_fd() const {
    assert(fds_[0] > 0);
    return fds_[0];
  }

  /**
   * @brief Get the write end of the pipe.
   *
   * @return int The write end of the pipe.
   */
  int writable_fd() const {
    assert(fds_[1] > 0);
    return fds_[1];
  }

  /**
   * @brief Close the read end of the pipe.
   *
   * @return task<void>
   */
  task<void> close_read() {
    co_await loop_->close(readable_fd());
    fds_[0] = -1;
  }

  /**
   * @brief Close the write end of the pipe.
   *
   * @return task<void>
   */
  task<void> close_write() {
    co_await loop_->close(writable_fd());
    fds_[1] = -1;
  }

  /**
   * @brief Close the read and write ends of the pipe.
   *
   * @return task<void>
   */
  task<void> close() {
    if (fds_[0] > 0) {
      co_await loop_->close(fds_[0]);
      fds_[0] = -1;
    }
    if (fds_[1] > 0) {
      co_await loop_->close(fds_[1]);
      fds_[1] = -1;
    }
  }

  /**
   * @brief Destroy the pipe object. If any end of the pipe is still open, it
   * will be closed.
   *
   */
  ~pipe() {
    if (fds_[0] > 0) {
      loop_->close_detach(fds_[0]);
    }
    if (fds_[1] > 0) {
      loop_->close_detach(fds_[1]);
    }
  }
};

} // namespace uringpp