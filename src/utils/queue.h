// Copyright (c) 2025 Juantgd. All Rights Reserved.

#ifndef JDOCS_UTILS_QUEUE_H_
#define JDOCS_UTILS_QUEUE_H_

#include <atomic>
#include <cstdint>

namespace jdocs {

template <typename T> class Queue {
public:
  Queue(uint32_t size) : size_(size) {
    array_ = new T *[size_];
    head_.store(0, std::memory_order_relaxed);
    tail_.store(0, std::memory_order_relaxed);
  }
  ~Queue() {
    if (array_) {
      for (uint32_t i = 0; i < size_; ++i) {
        if (array_[i])
          delete array_[i];
      }
      delete[] array_;
    }
  }

  void push_back(T *value) {
    uint64_t old_tail = tail_.load(std::memory_order_relaxed);
    uint64_t next_tail = (old_tail + 1) % size_;
    while (tail_.compare_exchange_strong(old_tail, next_tail,
                                         std::memory_order_seq_cst))
      ;
    array_[tail_] = value;
    tail_.store((old_tail + 1) % size_, std::memory_order_release);
  }

private:
  T **array_;
  uint32_t size_;
  alignas(64) std::atomic_uint64_t head_;
  alignas(64) std::atomic_uint64_t tail_;
};

} // namespace jdocs

#endif
