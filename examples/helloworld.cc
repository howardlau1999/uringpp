
#include <cassert>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>

#include <uringpp/uringpp.h>

uringpp::task<void> read_file(uringpp::event_loop &loop) {
  ::printf("Open file\n");
  int fd = co_await loop.openat(AT_FDCWD, "CMakeLists.txt", 0, O_RDONLY);
  assert(fd > 0);
  char buf[4096];
  ::printf("Read file\n");
  ssize_t n = co_await loop.read(fd, buf, sizeof(buf), 0);
  assert(n > 0);
  ::printf("%s\n", buf);
  ::printf("Close file\n");
  co_await loop.close(fd);
}

int main() {
  uringpp::event_loop loop;
  auto t = read_file(loop);
  while (!t.h_.done()) {
    loop.poll();
  }

  return 0;
}