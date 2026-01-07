// Copyright (c) 2025 Juantgd. All Rights Reserved.

#ifndef JDOCS_CORE_EVENT_LOOP_H_
#define JDOCS_CORE_EVENT_LOOP_H_

#include <liburing.h>

#include "buffer.h"
#include "context.h"
#include "timer.h"

namespace jdocs {

// 一些常量的定义，只能在该文件内所使用
namespace {
// io_uring实例队列最大条目数量
constexpr static uint32_t kQueueDepth = 2048;
// io_uring实例注册的文件描述符表大小
constexpr static uint32_t kFdTableSize = 2048;
// io_uring轮询线程闲置超时时间，ms为单位
// constexpr static uint32_t kThreadIdleTime = 2000;

// 闲置连接超时关闭时间，默认60s
constexpr static uint32_t kConnIdleTimeout = 60000;

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

  int prep_recv(int fd, uint32_t conn_id);

  // 无需获取固定缓冲区，用于发送较小的数据包
  int prep_send(int fd, uint32_t conn_id, void *data, size_t length);

  int prep_send_zc(int fd, uint32_t conn_id, void *data, uint16_t bidx,
                   size_t length, bool flag = false);

  int prep_close(int fd, uint32_t conn_id);

  // 准备一个跨线程消息，其中conn_id为目标线程的连接id
  int prep_cross_thread_msg(uint32_t conn_id, CTContext *context);

  int submit_cancel(int fd, uint32_t conn_id);

  // 获取发送缓冲区，用于填充发送数据
  // 若存在可用发送缓冲区，则设置buffer_ptr指向发送缓冲区地址，否则为NULL
  // 返回固定缓冲区索引，失败时返回-1
  int get_send_buffer(void **buffer_ptr);

  // 添加一个超时任务
  inline void AddTimer(TimeWheel::timer_node *timer, uint32_t millis) {
    time_wheel_->AddTimer(timer, millis);
  }

  struct io_uring_sqe *GetSqe();

private:
  void SetUpIoUring(uint32_t entries);
  void DestroyIoUring();

  // 准备下一次超时
  void __prep_timeout(timespec *ts);

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
  int handle_cross_thread_msg(struct io_uring_cqe *cqe);
  int handle_timeout(struct io_uring_cqe *cqe);

  // 缓冲池，用于管理接受/发送数据缓冲区
  std::unique_ptr<BufferPool> buffer_pool_;

  JdocsServer *server_;
  Worker *worker_;

  struct io_uring ring_;
  bool running_{false};

  // 时间轮，用于管理超时任务
  std::unique_ptr<TimeWheel> time_wheel_;

  // true则代表属于worker线程的事件循环，否则为master线程的事件循环
  bool flag_;
};

} // namespace jdocs

#endif
