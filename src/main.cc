// Copyright (c) 2025-2026 Juantgd. All Rights Reserved.

#include "core/server.h"

int main() {
  jdocs::JdocsServer server(7788);
  server.Run();
  return 0;
}
