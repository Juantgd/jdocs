// Copyright (c) 2025-2026 Juantgd. All Rights Reserved.

#ifndef JDOCS_UTILS_HELPERS_H_
#define JDOCS_UTILS_HELPERS_H_

#include <netinet/in.h>
#include <sys/socket.h>

#include <spdlog/spdlog.h>

namespace jdocs {

int create_listening_socket(int port);

static inline uint64_t get_current_millis() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return uint64_t(ts.tv_sec) * 1000 + uint64_t(ts.tv_nsec) / 1000000;
}

void timespec_add_millis(timespec *ts, uint64_t millis);

std::string get_datetime();

} // namespace jdocs

#endif
