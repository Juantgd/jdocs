// Copyright (c) 2025-2026 Juantgd. All Rights Reserved.

#ifndef JDOCS_INCLUDE_PROTOCOL_HANDLER_H_
#define JDOCS_INCLUDE_PROTOCOL_HANDLER_H_

#include <cstdlib>

namespace jdocs {

class TcpConnection;
// 应用层协议解析器接口类，所有应用层协议的解析器的实现都需要继承该类
class ProtocolHandler {
public:
  ProtocolHandler(TcpConnection *connection) : connection_(connection) {}
  virtual ~ProtocolHandler() = default;
  // 应用层协议处理函数
  virtual void RecvDataHandle(void *buffer, size_t length) = 0;
  virtual void TimeoutHandle() = 0;

protected:
  TcpConnection *connection_;
};

} // namespace jdocs

#endif
