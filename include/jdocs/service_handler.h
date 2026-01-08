// Copyright (c) 2025-2026 Juantgd. All Rights Reserved.

#ifndef JDOCS_INCLUDE_SERVICE_HANDLER_H_
#define JDOCS_INCLUDE_SERVICE_HANDLER_H_

#include <string>

namespace jdocs {

class TcpConnection;

class ServiceHandler {
public:
  ServiceHandler(TcpConnection *connection) : connection_(connection) {}
  virtual ~ServiceHandler() = default;

  virtual std::string handle(std::string data) = 0;

protected:
  TcpConnection *connection_;
};

} // namespace jdocs

#endif
