#include "uringpp/socket.h"

#include "uringpp/awaitable.h"
#include "uringpp/file.h"

namespace uringpp {

sqe_awaitable socket::tee(file const &out, size_t count, unsigned int flags) {
  return loop_->tee(fd_, out.fd(), count, flags);
}

sqe_awaitable socket::splice(loff_t off_in, file const &out, loff_t off_out,
                             size_t nbytes, unsigned flags) {
  return loop_->splice(fd_, off_in, out.fd(), off_out, nbytes, flags);
}

} // namespace uringpp