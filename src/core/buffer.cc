// Copyright (c) 2025-2026 Juantgd. All Rights Reserved.

#include "buffer.h"

#include <spdlog/spdlog.h>

#include "context.h"

namespace jdocs {

BufferPool::BufferPool(struct io_uring *ring)
    : ring_(ring), avaliable_buf_index_(kBufferEntriesMax, true) {
  int err;
  buf_ring_ = io_uring_setup_buf_ring(ring_, kBufferEntriesMax, bgid_, 0, &err);
  if (!buf_ring_) {
    spdlog::error("io_uring_setup_buf_ring failed. error: {}", strerror(-err));
    exit(EXIT_FAILURE);
  }
  err = io_uring_register_buffers_sparse(ring_, kBufferEntriesMax);
  if (err) {
    spdlog::error("io_uring_register_buffers_sparse failed. error: {}",
                  strerror(-err));
    exit(EXIT_FAILURE);
  }
  // 初始化接受缓冲池
  alloc_recv_buffers();
  // 初始化发送缓冲池
  alloc_send_buffers();
}

BufferPool::~BufferPool() {
  int ret = io_uring_free_buf_ring(ring_, buf_ring_, kBufferEntriesMax, bgid_);
  if (ret < 0) {
    spdlog::error("io_uring_free_buf_ring failed. error: {}", strerror(-ret));
    exit(EXIT_FAILURE);
  }
  for (auto p : recv_pool_) {
    free(p);
  }
  ret = io_uring_unregister_buffers(ring_);
  if (ret < 0) {
    spdlog::error("io_uring_unregister_buffers failed. error: {}",
                  strerror(-ret));
    exit(EXIT_FAILURE);
  }
  for (auto p : send_pool_) {
    free(p);
  }
}

// 通过bid获取缓冲池中对应的缓冲区
void *BufferPool::GetRecvBuffer(uint16_t bid) {
  uint16_t index = bid / kBufferCount;
  if (index > recv_pool_.size())
    return nullptr;
  return static_cast<char *>(recv_pool_[index]) +
         (kBufferSize * (bid & (kBufferCount - 1)));
}

// 将缓冲区返回缓冲池中，使内核有新的可用缓冲区
void BufferPool::ReplenishRecvBuffer(void *buffer_addr, uint16_t bid) {
  io_uring_buf_ring_add(buf_ring_, buffer_addr, kBufferSize, bid,
                        io_uring_buf_ring_mask(kBufferEntriesMax), 0);
  io_uring_buf_ring_advance(buf_ring_, 1);
}

// 扩容接收缓冲池大小
void BufferPool::alloc_recv_buffers() {
  if (recv_buffer_count_ == kBufferEntriesMax)
    return;
  void *buffer_addr;
  int ret = posix_memalign(&buffer_addr, 4096, kBlockSize);
  if (ret) {
    spdlog::error("posix_memalign failed. error: {}", strerror(ret));
  } else {
    recv_pool_.push_back(buffer_addr);
    for (uint16_t i = 0; i < kBufferCount; ++i) {
      io_uring_buf_ring_add(buf_ring_, buffer_addr, kBufferSize,
                            recv_buffer_count_++,
                            io_uring_buf_ring_mask(kBufferEntriesMax), i);
      buffer_addr = static_cast<char *>(buffer_addr) + kBufferSize;
    }
    io_uring_buf_ring_advance(buf_ring_, kBufferCount);
  }
}

// 对发送缓冲区进行扩容
void BufferPool::alloc_send_buffers() {
  if (send_buffer_count_ == kBufferEntriesMax)
    return;
  void *buffer_addr;
  int ret = posix_memalign(&buffer_addr, 4096, kBlockSize);
  if (ret) {
    spdlog::error("posix_memalign failed. error: {}", strerror(ret));
  } else {
    struct iovec iovecs[kBufferCount] = {};
    void *buffer_base = buffer_addr;
    for (uint16_t i = 0; i < kBufferCount; ++i) {
      iovecs[i].iov_base = buffer_addr;
      iovecs[i].iov_len = kBufferSize;
      buffer_addr = static_cast<char *>(buffer_addr) + kBufferSize;
    }
    __u64 tags[kBufferCount] = {};
    tags[0] = context_encode(__BUF_REL, 0, 0, send_buffer_count_);
    ret = io_uring_register_buffers_update_tag(ring_, send_buffer_count_,
                                               iovecs, tags, kBufferCount);
    if (ret != kBufferCount) {
      spdlog::error("io_uring_register_buffers_update_tag failed. error: {}",
                    strerror(-ret));
      free(buffer_base);
    } else {
      send_pool_.push_back(buffer_base);
      avaliable_buf_index_.RemoveIndexRange(send_buffer_count_, kBufferCount);
      send_buffer_count_ += kBufferCount;
    }
  }
}

// 当没有可用的发送缓冲区时，进行扩容操作，若发送缓冲区已经达到容量上限，则返回-1
int BufferPool::GetSendBufferIndex() {
  int buf_index = static_cast<int>(avaliable_buf_index_.GetAndSetIndex());
  if (buf_index == -1) {
    alloc_send_buffers();
    buf_index = static_cast<int>(avaliable_buf_index_.GetAndSetIndex());
  }
  return buf_index;
}

void *BufferPool::GetSendBuffer(uint16_t bidx) {
  if (bidx >= kBufferEntriesMax)
    return NULL;
  uint32_t index = bidx / kBufferCount;
  if (index >= send_pool_.size())
    return NULL;
  return static_cast<char *>(send_pool_[index]) +
         (kBufferSize * (bidx & (kBufferCount - 1)));
}

void BufferPool::ReplenishSendBuffer(uint16_t bidx) {
  if (bidx >= kBufferEntriesMax)
    return;
  // avaliable_buf_index_.push(bidx);
  if (!avaliable_buf_index_.RemoveIndex(bidx))
    spdlog::error("ReplenishSendBuffer Failed.");
}

} // namespace jdocs
