// Copyright (c) 2025 Juantgd. All Rights Reserved.

#ifndef JDOCS_CORE_CONTEXT_H_
#define JDOCS_CORE_CONTEXT_H_

#include <cstdint>

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
  __CROSS_MSG,
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

} // namespace jdocs

#endif
