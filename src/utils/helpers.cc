// Copyright (c) 2025-2026 Juantgd. All Rights Reserved.

#include "helpers.h"

#include <cerrno>

namespace jdocs {

int create_listening_socket(int port) {
  int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd < 0) {
    spdlog::error("create listening socket failed. error: {}", strerror(errno));
    return -1;
  }

  int enable = 1;
  int ret =
      setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
  if (ret < 0) {
    spdlog::error("setsockopt failed. error: {}", strerror(errno));
    return -1;
  }

  struct sockaddr_in addr{};
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);
  addr.sin_family = AF_INET;

  ret = bind(listen_fd, (const struct sockaddr *)&addr, sizeof(addr));
  if (ret < 0) {
    spdlog::error("bind failed. error: {}", strerror(errno));
    return -1;
  }

  ret = listen(listen_fd, 1024);
  if (ret < 0) {
    spdlog::error("listen failed. error: {}", strerror(errno));
    return -1;
  }

  return listen_fd;
}

void timespec_add_millis(timespec *ts, uint64_t millis) {
  ts->tv_sec += millis / 1000;
  ts->tv_nsec += (millis % 1000) * 1000000;
  if (ts->tv_nsec >= 1000000000) {
    ts->tv_sec++;
    ts->tv_nsec -= 1000000000;
  }
}

} // namespace jdocs
