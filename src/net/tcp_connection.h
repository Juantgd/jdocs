// Copyright (c) 2025 Juantgd. All Rights Reserved.

#ifndef JDOCS_NET_TCP_CONNECTION_H_
#define JDOCS_NET_TCP_CONNECTION_H_

#include <cstdint>
#include <memory>

#include "core/event_loop.h"
#include "protocol_handler.h"

namespace jdocs {

class TcpConnection {
public:
  explicit TcpConnection(EventLoop *event_loop, int fd, uint32_t conn_id);
  virtual ~TcpConnection() = default;

  // 不可拷贝，可移动
  TcpConnection(const TcpConnection &) = delete;
  TcpConnection &operator=(const TcpConnection &) = delete;
  TcpConnection(TcpConnection &&) = default;
  TcpConnection &operator=(TcpConnection &&) = default;

  // 当前连接阶段
  enum conn_stage_t { kConnStageHttp = 0, kConnStageWebsocket };

  inline int fd() const { return fd_; }
  inline uint32_t conn_id() const { return conn_id_; }
  inline bool closed() const { return closed_; }

  // 获取已经读取的字节数
  inline uint64_t recv_bytes() const { return recv_bytes_; }
  // 获取已经写入的字节数
  inline uint64_t send_bytes() const { return send_bytes_; }

  // 写操作完成处理函数，传入已写入的缓冲区地址和字节数
  virtual void SendHandle(void *buffer, size_t length);

  // 读操作完成处理函数，传入已读取的缓冲区地址和读取的字节数
  virtual void RecvHandle(void *buffer, size_t length);

  // 跨线程消息处理函数
  virtual void CrossHandle(void *data, size_t length);

  // 关闭操作
  void close() { closed_ = !event_loop_->__submit_cancel(fd_, conn_id_); }

  inline EventLoop *GetEventLoop() { return event_loop_; }

  void transition_stage(conn_stage_t stage);

private:
  // 直接文件描述符
  int fd_;
  bool closed_;
  uint64_t recv_bytes_;
  uint64_t send_bytes_;

  // 连接id
  uint32_t conn_id_;
  // 用户id，用于业务逻辑处理
  uint32_t user_id_;

  // 应用层协议处理类
  std::unique_ptr<ProtocolHandler> protocol_handler_;

  // 指向该文件描述符所属的事件循环
  EventLoop *event_loop_;
};

} // namespace jdocs

#endif
