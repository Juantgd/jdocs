// Copyright (c) 2025 Juantgd. All Rights Reserved.

#ifndef JDOCS_CORE_CONTEXT_H_
#define JDOCS_CORE_CONTEXT_H_

#include <atomic>
#include <cstdint>
#include <string>

#include <liburing.h>

namespace jdocs {

namespace {
constexpr static unsigned OP_SHIFT = 28;
}

enum {
  __ACCEPT = 0,
  __RECV,
  __SEND,
  __SEND_ZC,
  __CANCEL,
  __SHUTDOWN,
  __CLOSE,
  __FD_PASS,
  __CROSS_THREAD_MSG,
  __TIMEOUT,
  __BUF_REL,
  __NOP
};

struct Context {
  union {
    struct {
      // 高4位为操作码，低28位为连接id
      uint32_t op_conn_id;
      // 文件描述符
      uint16_t fd;
      // 缓冲区id
      uint16_t bid;
    };
    uint64_t val;
  };
};

// 用于跨线程传递消息的上下文，使用时动态分配
// 使用MSG_RING操作将高32位地址放入cqe->res字段中，低32位地址放入user_data中
struct CTContext {
  // 引用计数
  std::atomic<int> ref_count;
  // 消息发送方的连接id
  uint32_t snd_conn_id;
  // 承载着发送方想要发送的数据
  std::string message;

  CTContext(int refs, uint32_t conn_id, std::string msg)
      : ref_count(refs), snd_conn_id(conn_id), message(std::move(msg)) {}

  CTContext(const CTContext &) = delete;
  CTContext &operator=(const CTContext &) = delete;
};

static inline void release_context(CTContext *context) {
  // 当前引用计数为0，进行释放操作
  if (context->ref_count.fetch_sub(1, std::memory_order_acq_rel) == 1)
    delete context;
}

static inline uint32_t ctcontext_high_addr(CTContext *context) {
  return reinterpret_cast<uint64_t>(context) >> 32;
}

static inline uint32_t ctcontext_low_addr(CTContext *context) {
  return reinterpret_cast<uint64_t>(context) & 0x00000000FFFFFFFF;
}

static inline CTContext *get_ctcontext(uint32_t high_addr, uint32_t low_addr) {
  return reinterpret_cast<CTContext *>(
      (static_cast<uint64_t>(high_addr) << 32) | low_addr);
}

// 传递跨线程上下文低32位地址，封装为user_data
static inline uint64_t ctcontext_encode(uint32_t conn_id, uint32_t ctx_addr) {
  struct Context context = {.op_conn_id = (__CROSS_THREAD_MSG << OP_SHIFT) |
                                          (conn_id & 0x0FFFFFFF),
                            .fd = 0,
                            .bid = 0};
  context.val |= ctx_addr;
  return context.val;
}

static inline void user_data_encode(struct io_uring_sqe *sqe, int opcode,
                                    uint32_t conn_id, int fd, uint16_t bid) {
  struct Context context = {.op_conn_id =
                                (opcode << OP_SHIFT) | (conn_id & 0x0FFFFFFF),
                            .fd = static_cast<uint16_t>(fd),
                            .bid = bid};
  io_uring_sqe_set_data64(sqe, context.val);
}

static inline uint64_t context_encode(int opcode, uint32_t conn_id, int fd,
                                      uint16_t bid) {
  struct Context context = {.op_conn_id =
                                (opcode << OP_SHIFT) | (conn_id & 0x0FFFFFFF),
                            .fd = static_cast<uint16_t>(fd),
                            .bid = bid};
  return context.val;
}

static inline uint32_t cqe_to_conn_id(struct io_uring_cqe *cqe) {
  struct Context context = {.val = cqe->user_data};
  return context.op_conn_id & 0x0FFFFFFF;
}

static inline int cqe_to_op(struct io_uring_cqe *cqe) {
  struct Context context = {.val = cqe->user_data};
  return context.op_conn_id >> OP_SHIFT;
}

static inline uint16_t cqe_to_bid(struct io_uring_cqe *cqe) {
  struct Context context = {.val = cqe->user_data};
  return context.bid;
}

static inline uint16_t cqe_to_fd(struct io_uring_cqe *cqe) {
  struct Context context = {.val = cqe->user_data};
  return context.fd;
}

static inline uint32_t cqe_to_addr(struct io_uring_cqe *cqe) {
  struct Context context = {.val = cqe->user_data};
  return context.val & 0x00000000FFFFFFFF;
}

} // namespace jdocs

#endif
