// Copyright (c) 2025-2026 Juantgd. All Rights Reserved.

#include "tcp_connection.h"

#include <spdlog/spdlog.h>

#include "core/server.h"
#include "protocol/http/http_handler.h"
#include "protocol/websocket/websocket_handler.h"
#include "services/chat_service.h"
#include "services/document_service.h"

namespace jdocs {

TcpConnection::TcpConnection(EventLoop *event_loop, int fd, uint32_t conn_id)
    : fd_(fd), conn_id_(conn_id),
      protocol_handler_(std::make_unique<HttpHandler>(this)),
      event_loop_(event_loop) {}

void TcpConnection::close() {
  if (closed_)
    return;
  if (user_id_)
    JdocsServer::DelUserSession(user_id_);
  closed_ = !event_loop_->submit_cancel(fd_, conn_id_);
}

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

// 跨线程消息处函数
void TcpConnection::CrossThreadMsgHandle(void *data, size_t length) {
  if (stage_ == kConnStageWebsocket) {
    WebSocketHandler::send_data_frame(this, data, length);
  }
}

std::string TcpConnection::ServiceHandle(std::string data) {
  return service_handler_->handle(std::move(data));
}

bool TcpConnection::switch_service(
    std::string path,
    std::unordered_map<std::string, std::vector<std::string>> query_args) {
  auto it1 = router_.find(path);
  if (it1 == router_.end())
    return false;
  auto it2 = query_args.find("user_id");
  if (it2 == query_args.end())
    return false;
  try {
    user_id_ = std::stoi(it2->second[0]);
    JdocsServer::AddUserSession(user_id_, conn_id_);
  } catch (const std::invalid_argument &e) {
    spdlog::error("invalid user_id, error: {}", e.what());
    return false;
  } catch (const std::out_of_range &e) {
    spdlog::error("user_id out of range, error: {}", e.what());
    return false;
  }
  service_id_ = it1->second;
  switch (service_id_) {
  case kServiceNone:
    break;
  case kServiceChat: {
    service_handler_.reset(new ChatService(this));
    break;
  }
  case kServiceDocument: {
    service_handler_.reset(new DocumentService(this));
    break;
  }
  }
  return service_handler_->parse_parameters(std::move(query_args));
}

void TcpConnection::transition_stage(conn_stage_t stage) {
  stage_ = stage;
  switch (stage_) {
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

const std::unordered_map<std::string, TcpConnection::service_t>
    TcpConnection::router_ = {{"/chat", kServiceChat},
                              {"/document", kServiceDocument}};

} // namespace jdocs
