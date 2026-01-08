// Copyright (c) 2025-2026 Juantgd. All Rights Reserved.

#include "http_handler.h"

#include <string.h>

#include <openssl/evp.h>
#include <openssl/sha.h>
#include <spdlog/spdlog.h>

#include "net/tcp_connection.h"

namespace jdocs {

HttpHandler::HttpHandler(TcpConnection *connection)
    : ProtocolHandler(connection) {
  parser.Reset();
}

void HttpHandler::TimeoutHandle() { connection_->close(); }

// 用于生成sec-websocket-accept的值
int HttpHandler::generate_accept_key(const char *key, char *buffer) {
  u_char tmp[20];
  SHA1((u_char *)key, 60, tmp);
  return EVP_EncodeBlock((u_char *)buffer, tmp, 20);
}

void HttpHandler::send_response_101(const char *key) {
  void *send_buf;
  int bidx = connection_->GetEventLoop()->get_send_buffer(&send_buf);
  if (bidx == -1) {
    // 没有可用的发送缓冲区，说明目前服务器负载有点大，所以直接关闭该连接
    spdlog::error("not avaliable buffer to send. closing client.");
    connection_->close();
    return;
  }
  memcpy(send_buf, kHttpResponse101, sizeof(kHttpResponse101) - 1);
  generate_accept_key(key, (char *)send_buf + sizeof(kHttpResponse101) - 1);
  ((char *)send_buf)[sizeof(kHttpResponse101) + 27] = '\r';
  ((char *)send_buf)[sizeof(kHttpResponse101) + 28] = '\n';
  ((char *)send_buf)[sizeof(kHttpResponse101) + 29] = '\r';
  ((char *)send_buf)[sizeof(kHttpResponse101) + 30] = '\n';
  connection_->GetEventLoop()->prep_send_zc(
      connection_->fd(), connection_->conn_id(), send_buf, bidx,
      sizeof(kHttpResponse101) + 31);
}

// 生成400错误请求报文
void HttpHandler::send_response_400() {
  connection_->GetEventLoop()->prep_send(
      connection_->fd(), connection_->conn_id(), (void *)kHttpResponse400,
      sizeof(kHttpResponse400) - 1, true);
}

// 生成404请求资源不存在报文
void HttpHandler::send_response_404() {
  connection_->GetEventLoop()->prep_send(
      connection_->fd(), connection_->conn_id(), (void *)kHttpResponse404,
      sizeof(kHttpResponse404) - 1, true);
}

void HttpHandler::RecvDataHandle(void *buffer, size_t length) {
  parser.ParserExecute(buffer, length);
  if (parser.IsDone()) {
    void *send_buf;
    int bidx = connection_->GetEventLoop()->get_send_buffer(&send_buf);
    if (bidx == -1) {
      // 没有可用的发送缓冲区，说明目前服务器负载有点大，所以直接关闭该连接
      spdlog::error("not avaliable buffer to send. closing client.");
      connection_->close();
      return;
    }
    if (request_uri_parse()) {
      // 升级为websocket协议
      send_response_101(parser.websocket_key_);
      connection_->transition_stage(TcpConnection::kConnStageWebsocket);
    } else {
      // 请求的资源不存在
      send_response_404();
      spdlog::error("http: The requested resource does not exist.");
      connection_->close();
    }
  } else {
    // 解析失败可能是因为需要更多数据进行解析，或是遇到了解析错误
    if (parser.GetErrorCode()) {
      parsing_fail_handle();
    }
    // 需要更多数据进行解析，什么都不做
  }
}

bool HttpHandler::request_uri_parse() { return true; }

void HttpHandler::parsing_fail_handle() {
  spdlog::error("HTTP Protocol parser failed. error: {}", parser.GetError());
  send_response_400();
  connection_->close();
}

} // namespace jdocs
