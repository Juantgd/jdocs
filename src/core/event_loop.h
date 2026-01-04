// Copyright (c) 2025 Juantgd. All Rights Reserved.

#ifndef JDOCS_CORE_EVENT_LOOP_H_
#define JDOCS_CORE_EVENT_LOOP_H_

#include <liburing.h>

#include "buffer.h"

namespace jdocs {

// 一些常量的定义，只能在该文件内所使用
namespace {
// io_uring实例队列最大条目数量
constexpr static uint32_t kQueueDepth = 2048;
// io_uring实例注册的文件描述符表大小
constexpr static uint32_t kFdTableSize = 2048;
// io_uring轮询线程闲置超时时间，ms为单位
constexpr static uint32_t kThreadIdleTime = 2000;
} // namespace

class Worker;
class JdocsServer;

// 事件循环类，每个线程都维护着一个事件循环实例
class EventLoop {
public:
  explicit EventLoop(JdocsServer *server, Worker *worker, bool flag);
  ~EventLoop();

  // 不可拷贝，可移动
  EventLoop(const EventLoop &) = delete;
  EventLoop &operator=(const EventLoop &) = delete;
  EventLoop(EventLoop &&) = default;
  EventLoop &operator=(EventLoop &&) = default;

  inline struct io_uring *GetRingInstance() { return &ring_; }

  // 开始事件循环处理已完成事件
  int Run();

  int __prep_recv(int fd, uint32_t conn_id);

  // int __prep_send(int fd, uint32_t conn_id, void *data, uint16_t bid,
  //                 size_t length);

  int __prep_send_zc(int fd, uint32_t conn_id, void *data, uint16_t bidx,
                     size_t length, bool flag = false);

  int __prep_close(int fd, uint32_t conn_id);

  int __prep_cross_msg(uint32_t conn_id, int fd, uint16_t bid,
                       unsigned int length);

  int __submit_cancel(int fd, uint32_t conn_id);

  // 获取发送缓冲区，用于填充发送数据
  // 若存在可用发送缓冲区，则设置buffer_ptr指向发送缓冲区地址，否则为NULL
  // 返回固定缓冲区索引，失败时返回-1
  int __get_send_buffer(void **buffer_ptr);

  struct io_uring_sqe *GetSqe();

private:
  void SetUpIoUring(uint32_t entries);
  void DestroyIoUring();
  int EventHandler(struct io_uring_cqe *cqe);
  // 事件处理函数
  int handle_accept(struct io_uring_cqe *cqe);
  int handle_recv(struct io_uring_cqe *cqe);
  int handle_send(struct io_uring_cqe *cqe);
  int handle_send_zc(struct io_uring_cqe *cqe);
  int handle_cancel(struct io_uring_cqe *cqe);
  int handle_shutdown(struct io_uring_cqe *cqe);
  int handle_close(struct io_uring_cqe *cqe);
  int handle_fd_pass(struct io_uring_cqe *cqe);
  int handle_cross_msg(struct io_uring_cqe *cqe);
  int handle_timeout(struct io_uring_cqe *cqe);

  // 缓冲池，用于管理接受/发送数据缓冲区
  BufferPool *buffer_pool_;

  JdocsServer *server_;
  Worker *worker_;

  struct io_uring ring_;
  bool running_{false};

  // true则代表属于worker线程的事件循环，否则为master线程的事件循环
  bool flag_;
};

} // namespace jdocs

#endif
