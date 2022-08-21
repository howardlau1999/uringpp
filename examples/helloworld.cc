
#include <asm-generic/errno.h>
#include <cassert>
#include <cstdio>
#include <fcntl.h>
#include <future>
#include <sys/socket.h>
#include <unistd.h>

#include "uringpp/event_loop.h"
#include "uringpp/listener.h"
#include <uringpp/uringpp.h>

uringpp::task<void> echo_server(std::shared_ptr<uringpp::event_loop> loop) {
  auto listener = uringpp::listener::listen(loop, "0.0.0.0", "8888");
  auto handler = [loop](uringpp::socket socket) -> uringpp::task<void> {
    char buf[1024];
    int n = 0;
    while ((n = co_await socket.recv(buf, sizeof(buf))) > 0) {
      co_await socket.send(buf, n, MSG_NOSIGNAL);
    }
    ::printf("client fd=%d disconnected n=%d\n", socket.fd(), n);
  };
  while (true) {
    auto [addr, socket] = co_await listener.accept();
    ::printf("accepted connection fd=%d from %s:%d\n", socket.fd(), addr.ip().c_str(),
             addr.port());
    handler(std::move(socket)).detach();
  }
}

uringpp::task<void> http_client(std::shared_ptr<uringpp::event_loop> loop) {
  auto socket = co_await uringpp::socket::connect(loop, "baidu.com", "80");
  ::printf("connected fd=%d\n", socket.fd());
  char const *request =
      "GET / HTTP/1.1\r\nHost: baidu.com\r\nUser-Agent: uringpp/0.1\r\nAccept: "
      "*/*\r\nConnection: close\r\n\r\n";
  co_await socket.send(request, ::strlen(request));
  char buf[1024];
  int n = 0;
  while ((n = co_await socket.recv(buf, sizeof(buf))) > 0) {
    co_await loop->write(STDOUT_FILENO, buf, n, 0);
  }
  co_await socket.shutdown(SHUT_RDWR);
  co_await socket.close();
  ::printf("disconnected\n");
}

uringpp::task<void> read_file(std::shared_ptr<uringpp::event_loop> loop) {
  ::printf("Open file\n");
  auto file = co_await uringpp::file::open(loop, "CMakeLists.txt", 0, O_RDONLY);
  char buf[4096];
  ::printf("Read file\n");
  co_await file.read(buf, sizeof(buf), 0);
  ::printf("%s\n", buf);
  ::printf("Close file\n");
}

int main() {
  auto loop = uringpp::event_loop::create();
  loop->block_on(read_file(loop));
  http_client(loop).detach();
  loop->block_on(echo_server(loop));
  return 0;
}