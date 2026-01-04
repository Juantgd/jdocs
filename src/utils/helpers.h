// Copyright (c) 2025 Juantgd. All Rights Reserved.

#ifndef JDOCS_UTILS_HELPERS_H_
#define JDOCS_UTILS_HELPERS_H_

#include <netinet/in.h>
#include <sys/socket.h>

#include <spdlog/spdlog.h>

namespace jdocs {

int create_listening_socket(int port);

} // namespace jdocs

#endif
