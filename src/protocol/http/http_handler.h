// Copyright (c) 2025-2026 Juantgd. All Rights Reserved.

#ifndef JDOCS_PROTOCOL_HTTP_HANDLER_H_
#define JDOCS_PROTOCOL_HTTP_HANDLER_H_

#include "protocol/http/http_parser.h"
#include "protocol_handler.h"

namespace jdocs {

namespace {
#define kHttpResponse400                                                       \
  "HTTP/1.1 400 Bad Request\r\nServer: jdocs_server\r\nContent-Type: "         \
  "text/plain; charset-utf8\r\nContent-Length: 26\r\nConnection: "             \
  "Close\r\n\r\nInvalid Handshake Request!"
#define kHttpResponse101                                                       \
  "HTTP/1.1 101 Switching Protocols\r\nConnection: Upgrade\r\n"                \
  "Upgrade: websocket\r\n"                                                     \
  "Server: jdocs_server\r\n"                                                   \
  "Sec-WebSocket-Accept: "

} // namespace

class HttpHandler : public ProtocolHandler {
public:
  HttpHandler(TcpConnection *connection);
  ~HttpHandler() = default;

  void RecvDataHandle(void *buffer, size_t length) override;

  void TimeoutHandle() override;

private:
  // 用于生成sec-websocket-accept的值
  static int generate_accept_key(const char *key, char *buffer);

  // 生成101响应报文
  static size_t generate_response_101(void *buffer, const char *key);

  // 生成400错误请求报文
  static size_t generate_response_400(void *buffer);

  HttpParser parser;
};

} // namespace jdocs

#endif
