// Copyright (c) 2025-2026 Juantgd. All Rights Reserved.

#ifndef JDOCS_UTILS_BITMAP_H_
#define JDOCS_UTILS_BITMAP_H_

#include <cstdint>

namespace jdocs {

namespace {
constexpr static uint32_t kBitMapUnitSize = sizeof(uint64_t) * 8;
constexpr static uint32_t kBitMapUnitMask = kBitMapUnitSize - 1;
} // namespace

class BitMap final {
public:
  explicit BitMap(uint32_t bit_count, bool disable_flag = false);
  ~BitMap();

  // 禁止拷贝构造、赋值，允许移动拷贝、赋值
  BitMap(const BitMap &) = delete;
  BitMap &operator=(const BitMap &) = delete;
  BitMap(BitMap &&) = default;
  BitMap &operator=(BitMap &&) = default;

  // 位图操作函数
  uint32_t GetAndSetIndex();
  bool SetIndex(uint32_t index);
  void SetIndexRange(uint32_t begin_index, uint32_t count);
  bool RemoveIndex(uint32_t index);
  void RemoveIndexRange(uint32_t begin_index, uint32_t count);
  inline bool IsFull() const { return bit_used_count_ == bit_count_; }
  inline uint32_t capability() const { return bit_count_; }
  inline uint32_t size() const { return bit_used_count_; }
  void Clear();

private:
  uint64_t *bitmap_;
  uint32_t bit_used_count_{0};
  uint32_t bit_count_;
  uint32_t blocks_;
};

} // namespace jdocs

#endif
