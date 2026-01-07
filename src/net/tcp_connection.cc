// Copyright (c) 2025 Juantgd. All Rights Reserved.

#include "tcp_connection.h"

#include "protocol/http/http_handler.h"
#include "protocol/websocket/websocket_handler.h"

namespace jdocs {

TcpConnection::TcpConnection(EventLoop *event_loop, int fd, uint32_t conn_id)
    : fd_(fd), conn_id_(conn_id),
      protocol_handler_(std::make_unique<HttpHandler>(this)),
      event_loop_(event_loop) {}

void TcpConnection::SendHandle(size_t length) {
  if (closed_)
    return;
  // 检查所有数据是否已经发送完毕，否则继续提交发送请求
  send_bytes_ += length;
}

// 读操作完成处理函数，传入已读取的缓冲区地址和读取的字节数
void TcpConnection::RecvHandle(void *buffer, size_t length) {
  if (closed_)
    return;
  recv_bytes_ += length;
  protocol_handler_->RecvDataHandle(buffer, length);
}

void TcpConnection::CrossThreadHandle(void *data, size_t length) {}

void TcpConnection::transition_stage(conn_stage_t stage) {
  switch (stage) {
  case kConnStageHttp: {
    protocol_handler_.reset(new HttpHandler(this));
    break;
  }
  case kConnStageWebsocket: {
    protocol_handler_.reset(new WebSocketHandler(this));
    break;
  }
  }
}

} // namespace jdocs
