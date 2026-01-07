// Copyright (c) 2025 Juantgd. All Rights Reserved.

#include <spdlog/spdlog.h>

#include "core/server.h"

int main() {
  jdocs::JdocsServer server(7788);
  server.Run();
  return 0;
}
