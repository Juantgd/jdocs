// Copyright (c) 2025 Juantgd. All Rights Reserved.

#include "event_loop.h"

#include <cstdlib>
#include <cstring>

#include <liburing.h>
#include <spdlog/spdlog.h>

#include "protocol/websocket/websocket_handler.h"
#include "server.h"
#include "utils/helpers.h"

namespace jdocs {

EventLoop::EventLoop(JdocsServer *server, Worker *worker, bool flag)
    : server_(server), worker_(worker), flag_(flag) {
  SetUpIoUring(kQueueDepth);
  // 只有worker线程需要
  if (flag_) {
    buffer_pool_ = std::make_unique<BufferPool>(&ring_);
    time_wheel_ = std::make_unique<TimeWheel>();
    uint32_t count;
    timespec *tsv = time_wheel_->GetTimeoutCache(&count);
    // 启动时，批量提交超时请求
    for (uint32_t i = 0; i < count; ++i) {
      __prep_timeout(&tsv[i]);
    }
    // spdlog::info("[start]: {}", get_current_millis());
  }
}

EventLoop::~EventLoop() { DestroyIoUring(); }

int EventLoop::Run() {
  running_ = true;
  struct io_uring_cqe *cqe;
  int ret = 0;
  while (running_) {
    unsigned head, completion_count = 0;
    // 等待完成队列
    ret = io_uring_submit_and_wait(&ring_, 1);
    if (ret < 0) {
      if (ret == -EINTR)
        continue;
      running_ = false;
      spdlog::error("io_uring_wait_cqe failed. error msg: {}", strerror(-ret));
      break;
    }
    // 当完成队列中有完成条目，则批量获取完成条目，并对其进行处理
    io_uring_for_each_cqe(&ring_, head, cqe) {
      if (EventHandler(cqe))
        return -1;
      ++completion_count;
    }
    io_uring_cq_advance(&ring_, completion_count);
  }
  return 0;
}

int EventLoop::EventHandler(struct io_uring_cqe *cqe) {
  int ret = 0;
  // 获取操作码，调用对应的处理函数进行处理
  switch (cqe_to_op(cqe)) {
  case __ACCEPT:
    ret = handle_accept(cqe);
    break;
  case __RECV:
    ret = handle_recv(cqe);
    break;
  // case __SEND:
  //   ret = handle_send(cqe);
  //   break;
  case __SEND_ZC:
    ret = handle_send_zc(cqe);
    break;
  case __CANCEL:
    ret = handle_cancel(cqe);
    break;
  case __SHUTDOWN:
    ret = handle_shutdown(cqe);
    break;
  case __CLOSE:
    ret = handle_close(cqe);
    break;
  case __FD_PASS:
    ret = handle_fd_pass(cqe);
    break;
  case __CROSS_THREAD_MSG:
    ret = handle_cross_thread_msg(cqe);
    break;
  case __TIMEOUT:
    ret = handle_timeout(cqe);
    break;
  case __NOP:
    return 0;
  default:
    spdlog::error("Unknown Operation Code: {}", cqe_to_op(cqe));
    return -1;
  }
  return ret;
}

void EventLoop::SetUpIoUring(uint32_t entries) {
  struct io_uring_params params;
  memset(&params, 0, sizeof(struct io_uring_params));
  params.flags = IORING_SETUP_DEFER_TASKRUN | IORING_SETUP_SINGLE_ISSUER;
  // 设置轮询线程超时闲置时间，默认1000ms
  // params.sq_thread_idle = kThreadIdleTime;
  int ret = io_uring_queue_init_params(entries, &ring_, &params);
  if (ret < 0) {
    spdlog::error("io_uring_queue_init_params failed. error msg: {}",
                  strerror(-ret));
    exit(EXIT_FAILURE);
  }
  // 向io_uring实例中注册直接文件描述符表，默认2048个直接文件描述符
  ret = io_uring_register_files_sparse(&ring_, kFdTableSize);
  if (ret < 0) {
    spdlog::error("io_uring_register_files_sparse failed. error msg: {}",
                  strerror(-ret));
    exit(EXIT_FAILURE);
  }
  ret = io_uring_register_ring_fd(&ring_);
  if (ret != 1) {
    spdlog::error("io_uring_register_ring_fd failed. error msg: {}",
                  strerror(-ret));
    exit(EXIT_FAILURE);
  }
}

void EventLoop::DestroyIoUring() {
  int ret = io_uring_unregister_files(&ring_);
  if (ret < 0) {
    spdlog::error("io_uring_unregister_files failed. error msg: {}",
                  strerror(-ret));
    io_uring_queue_exit(&ring_);
    exit(EXIT_FAILURE);
  }
  io_uring_queue_exit(&ring_);
}

struct io_uring_sqe *EventLoop::GetSqe() {
  struct io_uring_sqe *sqe;
  do {
    sqe = io_uring_get_sqe(&ring_);
    if (sqe)
      break;
    io_uring_submit(&ring_);
  } while (1);
  return sqe;
}

int EventLoop::prep_recv(int fd, uint32_t conn_id) {
  struct io_uring_sqe *sqe = GetSqe();
  io_uring_prep_recv_multishot(sqe, fd, NULL, 0, 0);
  sqe->flags |= IOSQE_FIXED_FILE | IOSQE_BUFFER_SELECT;
  sqe->buf_group = buffer_pool_->GetBgid();
  user_data_encode(sqe, __RECV, conn_id, fd, 0);
  return 0;
}

// 准备向客户端fd发送数据
// int EventLoop::__prep_send(int fd, uint32_t conn_id, void *data, uint16_t
// bid,
//                            size_t length) {
//   struct io_uring_sqe *sqe = GetSqe();
//   io_uring_prep_send(sqe, fd, data, length, MSG_WAITALL | MSG_NOSIGNAL);
//   sqe->flags |= IOSQE_FIXED_FILE;
//   user_data_encode(sqe, __SEND, conn_id, fd, bid);
//   return 0;
// }

// 零拷贝发送操作
int EventLoop::prep_send_zc(int fd, uint32_t conn_id, void *data, uint16_t bidx,
                            size_t length, bool flag) {
  struct io_uring_sqe *sqe = GetSqe();
  io_uring_prep_send_zc(sqe, fd, data, length, MSG_WAITALL | MSG_NOSIGNAL, 0);
  sqe->buf_index = bidx;
  sqe->ioprio = IORING_RECVSEND_FIXED_BUF;
  sqe->flags |= IOSQE_FIXED_FILE;
  if (flag)
    sqe->flags |= IOSQE_IO_LINK;
  user_data_encode(sqe, __SEND_ZC, conn_id, fd, bidx);
  return 0;
}

int EventLoop::prep_close(int fd, uint32_t conn_id) {
  struct io_uring_sqe *sqe = GetSqe();
  io_uring_prep_shutdown(sqe, fd, SHUT_RDWR);
  sqe->flags |= IOSQE_FIXED_FILE | IOSQE_IO_LINK | IOSQE_CQE_SKIP_SUCCESS;
  user_data_encode(sqe, __SHUTDOWN, conn_id, fd, 0);
  sqe = GetSqe();
  io_uring_prep_close_direct(sqe, static_cast<unsigned int>(fd));
  user_data_encode(sqe, __CLOSE, conn_id, fd, 0);
  return 0;
}

// 准备向其他事件循环中的io_uring实例提交sqe，实现跨线程通讯
int EventLoop::prep_cross_thread_msg(uint32_t conn_id, CTContext *context) {
  struct io_uring_sqe *sqe = GetSqe();
  uint64_t user_data = ctcontext_encode(conn_id, ctcontext_low_addr(context));
  int ring_fd = server_->ConnectionIdToRingFd(conn_id);
  io_uring_prep_msg_ring(sqe, ring_fd, ctcontext_high_addr(context), user_data,
                         0);
  user_data_encode(sqe, __NOP, conn_id, 0, 0);
  return 0;
}

// 提交取消请求，准备关闭连接
int EventLoop::submit_cancel(int fd, uint32_t conn_id) {
  struct io_uring_sqe *sqe = GetSqe();
  io_uring_prep_cancel_fd(sqe, fd, IORING_ASYNC_CANCEL_FD_FIXED);
  user_data_encode(sqe, __CANCEL, conn_id, fd, 0);
  io_uring_submit(&ring_);
  return 0;
}

// 通过io_uring_prep_msg_ring_fd_alloc将连接的文件描述符传递给对应的ring实例
// TODO: 是否需要close_direct？
int EventLoop::handle_accept(struct io_uring_cqe *cqe) {
  if (cqe->res < 0) {
    spdlog::error("io_uring_prep_multishot_accept_direct failed. error: {}",
                  strerror(-cqe->res));
    return -1;
  }
  if (flag_) {
    spdlog::error("Cannot Accept New Connection In Worker Thread.");
    return -1;
  }
  spdlog::info("[master] New Connection Accepted, fd: {}", cqe->res);
  // 通过轮询的方式将新连接分发给不同的worker线程
  int ring_fd = server_->GetNextRingInstance()->ring_fd;
  uint32_t conn_id = server_->GetNextConnectionId();
  struct io_uring_sqe *sqe = GetSqe();
  uint64_t user_data = context_encode(__FD_PASS, conn_id, 0, 0);
  io_uring_prep_msg_ring_fd_alloc(sqe, ring_fd, cqe->res, user_data, 0);
  user_data_encode(sqe, __NOP, conn_id, cqe->res, 0);
  // 如果IORING_CQE_F_MORE标志未设置，则需要重新提交accept请求
  if (!(cqe->flags & IORING_CQE_F_MORE)) {
    sqe = GetSqe();
    io_uring_prep_multishot_accept_direct(sqe, server_->GetListeningFd(), NULL,
                                          NULL, 0);
    user_data_encode(sqe, __ACCEPT, 0, server_->GetListeningFd(), 0);
  }
  return 0;
}

// 接受主线程传递过来的直接文件描述符
int EventLoop::handle_fd_pass(struct io_uring_cqe *cqe) {
  if (cqe->res < 0) {
    if (cqe->res != -ENFILE) {
      spdlog::error("io_uring_prep_msg_ring_fd_alloc failed. error: {}",
                    strerror(-cqe->res));
      return -1;
    }
    spdlog::warn("The direct descriptor table in the ring is full.");
    return 0;
  }
  uint32_t conn_id = cqe_to_conn_id(cqe);
  spdlog::info("[{}] Accepted a new fd: {}, conn_id: {}", worker_->GetName(),
               cqe->res, conn_id);
  std::shared_ptr<TcpConnection> connection =
      std::make_shared<TcpConnection>(this, cqe->res, conn_id);
  worker_->AddConnection(conn_id, connection);
  AddTimer(connection->GetTimer(), kConnIdleTimeout);
  // 开始发起接受请求
  prep_recv(cqe->res, conn_id);
  spdlog::info("[{}] current time: {}", worker_->GetName(),
               get_current_millis());
  return 0;
}

int EventLoop::handle_recv(struct io_uring_cqe *cqe) {
  if (cqe->res < 0) {
    if (cqe->res == -ENOBUFS) {
      spdlog::info("[{}] no avaliable buffers", worker_->GetName());
      // 需要对缓冲池进行扩容，并重新提交接受数据请求
      buffer_pool_->alloc_recv_buffers();
      prep_recv(cqe_to_fd(cqe), cqe_to_conn_id(cqe));
    } else if (cqe->res != -ECANCELED) {
      spdlog::error("recv multishot failed. error: {}", strerror(-cqe->res));
      return -1;
    }
    return 0;
  }
  std::shared_ptr<TcpConnection> connection =
      worker_->GetConnection(cqe_to_conn_id(cqe));
  int fd = cqe_to_fd(cqe);
  if (!(cqe->flags & IORING_CQE_F_BUFFER)) {
    // 说明客户端关闭连接
    connection->close();
    return 0;
  }
  // 更新连接超时定时器
  AddTimer(connection->GetTimer(), kConnIdleTimeout);
  uint16_t bid = cqe->flags >> IORING_CQE_BUFFER_SHIFT;
  void *recv_buf = buffer_pool_->GetRecvBuffer(bid);
  if (!recv_buf) {
    spdlog::error("[{}] No selected buffer.", worker_->GetName());
    return -1;
  }
  spdlog::info("[{}] receive {} bytes from fd: {}, bid: {}", worker_->GetName(),
               cqe->res, fd, bid);

  // 对该连接对象进行业务处理
  if (!connection->closed()) {
    spdlog::info("[{}] call receive handle.", worker_->GetName());
    connection->RecvHandle(recv_buf, static_cast<size_t>(cqe->res));
  }

  // 处理完毕后需要将接收缓冲区放回缓存池中
  buffer_pool_->ReplenishRecvBuffer(recv_buf, bid);

  if (!(cqe->flags & IORING_CQE_F_MORE)) {
    prep_recv(fd, cqe_to_conn_id(cqe));
  }

  return 0;
}

// 复用接收缓冲区，发送操作完成后需要进行补充接收缓冲区
int EventLoop::handle_send(struct io_uring_cqe *cqe) {
  if (cqe->res < 0) {
    spdlog::error("send operation failed. error: {}", strerror(-cqe->res));
    return -1;
  }
  int fd = cqe_to_fd(cqe);
  uint16_t bid = cqe_to_bid(cqe);
  void *buffer_addr = buffer_pool_->GetRecvBuffer(bid);
  if (buffer_addr == NULL) {
    spdlog::error("[{}] invalid buffer, bid: {}", worker_->GetName(), bid);
    return -1;
  }
  std::shared_ptr<TcpConnection> connection =
      worker_->GetConnection(cqe_to_conn_id(cqe));
  if (!connection->closed())
    connection->SendHandle(buffer_addr, static_cast<size_t>(cqe->res));
  spdlog::info("[{}] send {} bytes to fd: {}", worker_->GetName(), cqe->res,
               fd);
  buffer_pool_->ReplenishRecvBuffer(buffer_pool_->GetRecvBuffer(bid), bid);
  return 0;
}

int EventLoop::handle_send_zc(struct io_uring_cqe *cqe) {
  if (cqe->res < 0 && !(cqe->flags & IORING_CQE_F_NOTIF)) {
    spdlog::error("send zero copy operation failed. error: {}",
                  strerror(-cqe->res));
    return -1;
  }
  int fd = cqe_to_fd(cqe);
  uint16_t bidx = cqe_to_bid(cqe);
  // 此时可以复用该发送缓冲区
  if (cqe->flags & IORING_CQE_F_NOTIF) {
    spdlog::info("[{}] send buffer recycle, buffer_index: {}",
                 worker_->GetName(), bidx);
    buffer_pool_->ReplenishSendBuffer(bidx);
  } else {
    std::shared_ptr<TcpConnection> connection =
        worker_->GetConnection(cqe_to_conn_id(cqe));
    void *buffer_addr = buffer_pool_->GetRecvBuffer(bidx);
    if (buffer_addr == NULL) {
      spdlog::error("[{}] invalid buffer, buffer_index: {}", worker_->GetName(),
                    bidx);
      return -1;
    }
    if (!connection->closed())
      connection->SendHandle(buffer_addr, static_cast<size_t>(cqe->res));
    spdlog::info("[{}] send {} bytes to fd: {}", worker_->GetName(), cqe->res,
                 fd);
  }
  return 0;
}

// 如果触发了该事件的处理函数，则代表shutdown失败了，此时应该直接关闭连接
int EventLoop::handle_shutdown(struct io_uring_cqe *cqe) {
  if (cqe->res < 0) {
    spdlog::error("io_uring_prep_shutdown failed. error: {}",
                  strerror(-cqe->res));
    struct io_uring_sqe *sqe = GetSqe();
    io_uring_prep_close_direct(sqe, cqe_to_fd(cqe));
    user_data_encode(sqe, __CLOSE, 0, cqe_to_fd(cqe), 0);
  }
  return 0;
}

int EventLoop::handle_close(struct io_uring_cqe *cqe) {
  if (cqe->res < 0) {
    spdlog::error("io_uring_prep_close_direct failed. error: {}",
                  strerror(-cqe->res));
    return -1;
  }
  if (!flag_) {
    spdlog::info("[master] connection closed, fd: {}", cqe_to_fd(cqe));
    return 0;
  }
  spdlog::info("[{}] connection closed, fd: {}, conn_id: {}",
               worker_->GetName(), cqe_to_fd(cqe), cqe_to_conn_id(cqe));

  spdlog::info("[{}] current time: {}", worker_->GetName(),
               get_current_millis());
  worker_->DelConnection(cqe_to_conn_id(cqe));
  return 0;
}

// 当取消操作完成后，需要对连接进行关闭操作
int EventLoop::handle_cancel(struct io_uring_cqe *cqe) {
  spdlog::info("[{}] got cancel fd: {}", worker_->GetName(), cqe_to_fd(cqe));
  prep_close(cqe_to_fd(cqe), cqe_to_conn_id(cqe));
  return 0;
}

// 处理跨线程消息
int EventLoop::handle_cross_thread_msg(struct io_uring_cqe *cqe) {
  // 通过获取cqe->res中存放的高32位地址和user_data中低32位地址
  // 得到实际的跨线程上下文对象地址
  uint32_t low_addr = cqe_to_addr(cqe);
  uint32_t high_addr = static_cast<uint32_t>(cqe->res);
  CTContext *context = get_ctcontext(high_addr, low_addr);
  if (!context) {
    spdlog::error("[{}] got a null cross thread message", worker_->GetName());
    release_context(context);
    return 0;
  }
  spdlog::info("[{}] got a cross thread message, sender: {}",
               worker_->GetName(), context->snd_conn_id);
  std::shared_ptr<TcpConnection> connection =
      worker_->GetConnection(cqe_to_conn_id(cqe));
  if (!connection || connection->closed()) {
    spdlog::info("[{}] conn_id: {} not online", worker_->GetName(),
                 cqe_to_conn_id(cqe));
    release_context(context);
    return 0;
  }
  if (connection->GetConnStage() == TcpConnection::kConnStageWebsocket) {
    WebSocketHandler::send_data_frame(connection.get(), context->message.data(),
                                      context->message.size());
  }
  release_context(context);
  return 0;
}

// 准备下一次超时
void EventLoop::__prep_timeout(timespec *ts) {
  io_uring_sqe *sqe = GetSqe();
  // 使用绝对时间
  io_uring_prep_timeout(sqe, (struct __kernel_timespec *)ts, 0,
                        IORING_TIMEOUT_ABS);
  user_data_encode(sqe, __TIMEOUT, 0, 0, 0);
}

// 定时器事件处理函数，处理当前超时的事件
int EventLoop::handle_timeout(struct io_uring_cqe *cqe) {
  if (cqe->res != -ETIME) {
    spdlog::error("io_uring_prep_timeout failed. error: {}",
                  strerror(-cqe->res));
    return -1;
  }
  // spdlog::info("[{}][tick]: {}", worker_->GetName(), get_current_millis());
  // 开始处理超时事件，处理超时任务
  time_wheel_->Update();
  __prep_timeout(time_wheel_->GetNextTimeout());
  return 0;
}

int EventLoop::get_send_buffer(void **buffer_ptr) {
  int bidx = buffer_pool_->GetSendBufferIndex();
  if (bidx == -1) {
    *buffer_ptr = NULL;
  } else {
    *buffer_ptr = buffer_pool_->GetSendBuffer(static_cast<uint16_t>(bidx));
  }
  return bidx;
}

} // namespace jdocs
