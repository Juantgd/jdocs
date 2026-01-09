// Copyright (c) 2025-2026 Juantgd. All Rights Reserved.

#ifndef JDOCS_INCLUDE_SERVICE_HANDLER_H_
#define JDOCS_INCLUDE_SERVICE_HANDLER_H_

#include <string>
#include <unordered_map>
#include <vector>

namespace jdocs {

class TcpConnection;

class ServiceHandler {
public:
  ServiceHandler(TcpConnection *connection) : connection_(connection) {}
  virtual ~ServiceHandler() = default;

  // 解析服务开始处理所需要的参数，缺少必要参数时返回false
  virtual bool parse_parameters(
      std::unordered_map<std::string, std::vector<std::string>> args) = 0;

  virtual std::string handle(std::string data) = 0;

protected:
  TcpConnection *connection_;
};

} // namespace jdocs

#endif
