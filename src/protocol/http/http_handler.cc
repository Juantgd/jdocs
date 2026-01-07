// Copyright (c) 2025-2026 Juantgd. All Rights Reserved.

#include "http_handler.h"

#include <openssl/evp.h>
#include <openssl/sha.h>

#include "net/tcp_connection.h"

namespace jdocs {

HttpHandler::HttpHandler() { parser.Reset(); }

// 用于生成sec-websocket-accept的值
int HttpHandler::generate_accept_key(const char *key, char *buffer) {
  u_char tmp[20];
  SHA1((u_char *)key, 60, tmp);
  return EVP_EncodeBlock((u_char *)buffer, tmp, 20);
}

size_t HttpHandler::generate_response_101(void *buffer, const char *key) {
  memcpy(buffer, kHttpResponse101, sizeof(kHttpResponse101) - 1);
  generate_accept_key(key, (char *)buffer + sizeof(kHttpResponse101) - 1);
  ((char *)buffer)[sizeof(kHttpResponse101) + 27] = '\r';
  ((char *)buffer)[sizeof(kHttpResponse101) + 28] = '\n';
  ((char *)buffer)[sizeof(kHttpResponse101) + 29] = '\r';
  ((char *)buffer)[sizeof(kHttpResponse101) + 30] = '\n';
  ((char *)buffer)[sizeof(kHttpResponse101) + 31] = 0;
  return sizeof(kHttpResponse101) + 31;
}

// 生成400错误请求报文
size_t HttpHandler::generate_response_400(void *buffer) {
  memcpy(buffer, kHttpResponse400, sizeof(kHttpResponse400) - 1);
  return sizeof(kHttpResponse400) - 1;
}

void HttpHandler::RecvDataHandle(TcpConnection *connection, void *buffer,
                                 size_t length) {
  parser.ParserExecute(buffer, length);
  if (parser.IsDone()) {
    void *send_buf;
    int bidx = connection->GetEventLoop()->get_send_buffer(&send_buf);
    if (bidx != -1) {
      size_t prep_send_bytes =
          generate_response_101(send_buf, parser.websocket_key_);
      connection->GetEventLoop()->prep_send_zc(
          connection->fd(), connection->conn_id(), send_buf,
          static_cast<uint16_t>(bidx), prep_send_bytes);
      // 升级为websocket协议
      connection->transition_stage(TcpConnection::kConnStageWebsocket);
    } else {
      // 没有可用的发送缓冲区，说明目前服务器负载有点大，所以直接关闭该连接
      spdlog::error("not avaliable buffer to send. closing client.");
      connection->close();
    }
  } else {
    // 解析失败可能是因为需要更多数据进行解析，或是遇到了解析错误
    if (parser.GetErrorCode()) {
      spdlog::error("HTTP Protocol parser failed. error: {}",
                    parser.GetError());
      void *send_buf;
      int bidx = connection->GetEventLoop()->get_send_buffer(&send_buf);
      if (bidx != -1) {
        size_t prep_send_bytes = generate_response_400(send_buf);
        connection->GetEventLoop()->prep_send_zc(
            connection->fd(), connection->conn_id(), send_buf,
            static_cast<uint16_t>(bidx), prep_send_bytes, true);
      } else {
        spdlog::error("not avaliable buffer to send. closing client.");
      }
      connection->close();
    }
    // 需要更多数据进行解析，什么都不做
  }
}

} // namespace jdocs
