// Copyright (c) 2025 Juantgd. All Rights Reserved.

#ifndef JDOCS_CORE_BUFFER_H_
#define JDOCS_CORE_BUFFER_H_

#include <cstdint>
#include <vector>

#include <liburing.h>

#include <spdlog/spdlog.h>

#include "utils/bitmap.h"

namespace jdocs {

// 缓冲区块大小，默认2048字节
constexpr const uint32_t kBufferSize = 2048;
namespace {
// 线程缓冲区条目最大数量
constexpr static uint32_t kBufferEntriesMax = 1 << 14;
// 缓冲池扩容大小（块大小），默认512KB
constexpr static uint32_t kBlockSize = 2048 * 256;
// 将块分割为缓冲区的数量
constexpr static uint32_t kBufferCount = kBlockSize / kBufferSize;
} // namespace

// 提供recv/send操作所需要的缓冲区
// 其中发送缓冲区通过注册固定缓冲区以便后续使用零拷贝操作
class BufferPool {
public:
  BufferPool(struct io_uring *ring);
  ~BufferPool();

  // 通过bid获取缓冲池中对应的缓冲区
  void *GetRecvBuffer(uint16_t bid);

  // 获取可用的发送缓冲区下标，没有则返回-1
  int GetSendBufferIndex();

  // 通过缓冲区下标获取发送缓冲区
  void *GetSendBuffer(uint16_t bidx);

  // 补充发送缓冲池，使其能被下一个发送请求所使用
  void ReplenishSendBuffer(uint16_t bidx);

  // 将缓冲区返回缓冲池中，使内核有新的可用缓冲区
  void ReplenishRecvBuffer(void *buffer_addr, uint16_t bid);

  // 扩容缓冲池大小
  void alloc_recv_buffers();

  inline uint16_t GetBgid() const { return bgid_; }

private:
  void alloc_send_buffers();
  // 当前接收缓冲区数量，最大不超过2^15
  uint16_t recv_buffer_count_{0};
  // 当前发送缓冲区数量，最大数量限制同上
  uint16_t send_buffer_count_{0};
  // 缓冲组id
  uint16_t bgid_{1};
  struct io_uring *ring_;
  struct io_uring_buf_ring *buf_ring_;
  std::vector<void *> recv_pool_;
  std::vector<void *> send_pool_;
  BitMap avaliable_buf_index_;
};

} // namespace jdocs

#endif
