// Copyright (c) 2025 Juantgd. All Rights Reserved.

#include "bitmap.h"

#include <cstring>

namespace jdocs {

BitMap::BitMap(uint32_t bit_count, bool disable_flag) : bit_count_(bit_count) {
  blocks_ = (bit_count + (kBitMapUnitSize - 1)) / kBitMapUnitSize;
  bitmap_ = new uint64_t[blocks_];
  // 若设置了禁用标志，则代表初始化时位图所有资源不可用
  if (disable_flag) {
    memset(bitmap_, 0xFF, blocks_ * sizeof(uint64_t));
    bit_used_count_ = bit_count;
  } else {
    memset(bitmap_, 0, blocks_ * sizeof(uint64_t));
  }
}

BitMap::~BitMap() { delete[] bitmap_; }

uint32_t BitMap::GetAndSetIndex() {
  if (IsFull())
    return static_cast<uint32_t>(-1);
  for (uint32_t i = 0; i != blocks_; ++i) {
    // 如果找到有可用索引
    if (bitmap_[i] != ~0ULL) {
      int index = __builtin_ctzll(~bitmap_[i]);
      uint32_t ret = i * kBitMapUnitSize + static_cast<uint32_t>(index);
      if (ret < bit_count_) {
        bitmap_[i] |= (1ULL << index);
        ++bit_used_count_;
        return ret;
      } else {
        break;
      }
    }
  }
  return static_cast<uint32_t>(-1);
}

bool BitMap::SetIndex(uint32_t index) {
  if (index >= bit_count_)
    return false;
  uint32_t block = index / kBitMapUnitSize;
  uint64_t bit_mask = (1ULL << (index & kBitMapUnitMask));
  if (bitmap_[block] & bit_mask)
    return false;
  bitmap_[block] |= bit_mask;
  ++bit_used_count_;
  return true;
}

void BitMap::SetIndexRange(uint32_t begin_index, uint32_t count) {
  if (count == 0 || begin_index >= bit_count_ ||
      begin_index + count > bit_count_)
    return;

  uint32_t end_index = begin_index + count;
  uint32_t end_block = (end_index - 1) / kBitMapUnitSize;
  uint32_t begin_block = begin_index / kBitMapUnitSize;
  uint64_t bitmask = ~((1ULL << (begin_index & kBitMapUnitMask)) - 1);

  if (begin_block == end_block) {
    uint64_t mask = ((1ULL << (end_index & kBitMapUnitMask)) - 1);
    if (mask == 0)
      mask = ~0ULL;
    bitmask &= mask;
  }

  uint64_t block = bitmap_[begin_block] & bitmask;
  if (block != 0) {
    bit_used_count_ -= static_cast<uint32_t>(__builtin_popcountll(block));
  }
  bitmap_[begin_block] |= bitmask;

  for (uint32_t i = begin_block + 1; i < end_block; ++i) {
    if (bitmap_[i] != 0) {
      bit_used_count_ -=
          static_cast<uint32_t>(__builtin_popcountll(bitmap_[i]));
    }
    bitmap_[i] = 0xFFFFFFFFFFFFFFFF;
  }

  if (begin_block != end_block) {
    bitmask = ((1ULL << (end_index & kBitMapUnitMask)) - 1);
    if (bitmask == 0)
      bitmask = ~0ULL;
    block = bitmap_[end_block] & bitmask;
    if (block != 0) {
      bit_used_count_ -= static_cast<uint32_t>(__builtin_popcountll(block));
    }
    bitmap_[end_block] |= bitmask;
  }

  bit_used_count_ += count;
}

bool BitMap::RemoveIndex(uint32_t index) {
  if (index >= bit_count_)
    return false;
  uint32_t block = index / kBitMapUnitSize;
  uint64_t bit_mask = (1ULL << (index & kBitMapUnitMask));
  if (bitmap_[block] & bit_mask) {
    bitmap_[block] &= ~bit_mask;
    --bit_used_count_;
    return true;
  }
  return false;
}

void BitMap::RemoveIndexRange(uint32_t begin_index, uint32_t count) {
  if (count == 0 || begin_index >= bit_count_ ||
      begin_index + count > bit_count_)
    return;

  uint32_t end_index = begin_index + count;
  uint32_t end_block = (end_index - 1) / kBitMapUnitSize;
  uint32_t begin_block = begin_index / kBitMapUnitSize;
  uint64_t bitmask = ~((1ULL << (begin_index & kBitMapUnitMask)) - 1);

  if (begin_block == end_block) {
    uint64_t mask = ((1ULL << (end_index & kBitMapUnitMask)) - 1);
    if (mask == 0)
      mask = ~0ULL;
    bitmask &= mask;
  }

  uint64_t block = bitmap_[begin_block] & bitmask;
  if (block != 0) {
    bit_used_count_ -= static_cast<uint32_t>(__builtin_popcountll(block));
  }
  bitmap_[begin_block] &= ~bitmask;

  for (uint32_t i = begin_block + 1; i < end_block; ++i) {
    if (bitmap_[i] != 0) {
      bit_used_count_ -=
          static_cast<uint32_t>(__builtin_popcountll(bitmap_[i]));
    }
    bitmap_[i] = 0;
  }

  if (begin_block != end_block) {
    bitmask = ((1ULL << (end_index & kBitMapUnitMask)) - 1);
    if (bitmask == 0)
      bitmask = ~0ULL;
    block = bitmap_[end_block] & bitmask;
    if (block != 0) {
      bit_used_count_ -= static_cast<uint32_t>(__builtin_popcountll(block));
    }
    bitmap_[end_block] &= ~bitmask;
  }
}

void BitMap::Clear() {
  bit_used_count_ = 0;
  memset(bitmap_, 0, blocks_ * sizeof(uint64_t));
}

} // namespace jdocs
