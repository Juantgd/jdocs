// Copyright (c) 2025 Juantgd. All Rights Reserved.

#ifndef JDOCS_NET_TCP_CONNECTION_H_
#define JDOCS_NET_TCP_CONNECTION_H_

#include <cstdint>

#include "core/event_loop.h"
#include "parser.h"
#include "protocol/http/http_parser.h"

namespace jdocs {

class TcpConnection {
public:
  explicit TcpConnection(EventLoop *event_loop, int fd, uint32_t conn_id)
      : fd_(fd), conn_id_(conn_id), parser_(new HttpParser),
        event_loop_(event_loop) {
    parser_->Reset();
  }
  ~TcpConnection();

  // 不可拷贝，可移动
  TcpConnection(const TcpConnection &) = delete;
  TcpConnection &operator=(const TcpConnection &) = delete;
  TcpConnection(TcpConnection &&) = default;
  TcpConnection &operator=(TcpConnection &&) = default;

  inline int fd() const { return fd_; }
  inline bool closed() const { return closed_; }

  // 获取已经读取的字节数
  inline uint64_t recv_bytes() const { return recv_bytes_; }
  // 获取已经写入的字节数
  inline uint64_t send_bytes() const { return send_bytes_; }

  // 写操作完成处理函数，传入已写入的缓冲区地址和字节数
  void SendHandle(void *buffer, size_t length);

  // 读操作完成处理函数，传入已读取的缓冲区地址和读取的字节数
  void RecvHandle(void *buffer, size_t length);

  // 发送消息函数，用于跨线程信息传递
  void SendMessage(void *data, size_t length);

  // 关闭操作
  void close() { closed_ = !event_loop_->__submit_cancel(fd_, conn_id_); }

private:
  // 当前连接阶段
  enum class stage_t { kStageHttp = 0, kStageWebsocket };
  // 连接状态子阶段
  enum class substage_t {
    kSubStageWebSocketEstablished = 0,
    kSubStageWebSocketClosing,
    kSubStageWebSocketClosed
  };

  void __http_stage_handle(void *buffer, size_t length);
  void __websocket_stage_handle(void *buffer, size_t length);
  void __transition_stage(stage_t stage);
  void __transition_substage(substage_t substage);

  stage_t stage_;
  substage_t substage_;

  // 直接文件描述符
  int fd_;
  bool closed_;
  uint64_t recv_bytes_;
  uint64_t send_bytes_;

  // 连接id
  uint32_t conn_id_;
  // 用户id，用于业务逻辑处理
  uint32_t user_id_;

  // 协议解析器
  Parser *parser_;

  // 指向该文件描述符所属的事件循环
  EventLoop *event_loop_;
};

} // namespace jdocs

#endif
