// Copyright (c) 2025 Juantgd. All Rights Reserved.

#include "utils/bitmap.h"

#include <gtest/gtest.h>

// 位图类功能测试

TEST(BitMapTest, BitMapTestBasic) {
  {
    jdocs::BitMap map(68);
    ASSERT_EQ(map.IsFull(), false);
    ASSERT_EQ(map.capability(), 68);

    ASSERT_EQ(map.SetIndex(68), false);
    ASSERT_EQ(map.size(), 0);
    ASSERT_EQ(map.SetIndex(67), true);
    ASSERT_EQ(map.size(), 1);

    ASSERT_EQ(map.GetAndSetIndex(), 0);
    ASSERT_EQ(map.GetAndSetIndex(), 1);
    ASSERT_EQ(map.GetAndSetIndex(), 2);
    ASSERT_EQ(map.GetAndSetIndex(), 3);
    ASSERT_EQ(map.size(), 5);

    ASSERT_EQ(map.RemoveIndex(4), false);
    ASSERT_EQ(map.RemoveIndex(68), false);
    ASSERT_EQ(map.RemoveIndex(3), true);
    ASSERT_EQ(map.RemoveIndex(2), true);
    ASSERT_EQ(map.RemoveIndex(1), true);
    ASSERT_EQ(map.size(), 2);
    ASSERT_EQ(map.GetAndSetIndex(), 1);
    ASSERT_EQ(map.GetAndSetIndex(), 2);
    ASSERT_EQ(map.GetAndSetIndex(), 3);
    ASSERT_EQ(map.size(), 5);
  }
  {
    jdocs::BitMap map(144);
    for (uint32_t i = 0; i != 144; ++i) {
      uint32_t index = map.GetAndSetIndex();
      ASSERT_EQ(index, i) << "Expect index: " << i << " but " << index;
    }
    ASSERT_EQ(map.size(), 144);
    for (uint32_t i = 0; i != 144; ++i) {
      ASSERT_EQ(map.RemoveIndex(i), true);
    }
    ASSERT_EQ(map.RemoveIndex(0), false);
    ASSERT_EQ(map.size(), 0);
  }
}

TEST(BitMapTest, BitMapRangeSetAndRemoveTest) {
  {
    jdocs::BitMap map(64);
    ASSERT_EQ(map.size(), 0);

    ASSERT_EQ(map.SetIndex(4), true);
    ASSERT_EQ(map.SetIndex(7), true);

    ASSERT_EQ(map.size(), 2);

    map.SetIndexRange(0, 8);
    ASSERT_EQ(map.size(), 8);
    ASSERT_EQ(map.GetAndSetIndex(), 8);
    ASSERT_EQ(map.size(), 9);

    map.RemoveIndexRange(0, 9);
    ASSERT_EQ(map.size(), 0);

    // map.Clear();
    // ASSERT_EQ(map.size(), 0);

    ASSERT_EQ(map.SetIndex(10), true);
    ASSERT_EQ(map.SetIndex(17), true);
    ASSERT_EQ(map.SetIndex(13), true);
    ASSERT_EQ(map.SetIndex(23), true);
    ASSERT_EQ(map.SetIndex(30), true);
    ASSERT_EQ(map.size(), 5);

    map.SetIndexRange(8, 32);
    ASSERT_EQ(map.size(), 32);

    ASSERT_EQ(map.SetIndex(2), true);
    ASSERT_EQ(map.size(), 33);
    map.RemoveIndexRange(8, 56);
    ASSERT_EQ(map.size(), 1);
  }
  {
    jdocs::BitMap map(128);

    ASSERT_EQ(map.SetIndex(60), true);
    ASSERT_EQ(map.SetIndex(62), true);
    ASSERT_EQ(map.SetIndex(67), true);
    ASSERT_EQ(map.SetIndex(69), true);
    ASSERT_EQ(map.SetIndex(64), true);
    ASSERT_EQ(map.size(), 5);

    map.SetIndexRange(60, 10);
    ASSERT_EQ(map.size(), 10);

    map.RemoveIndexRange(50, 15);
    ASSERT_EQ(map.size(), 5);

    map.RemoveIndexRange(40, 40);
    ASSERT_EQ(map.size(), 0);
  }
  {
    jdocs::BitMap map(1024, true);
    ASSERT_EQ(map.size(), 1024);

    ASSERT_EQ(map.GetAndSetIndex(), -1);

    map.RemoveIndexRange(0, 256);
    ASSERT_EQ(map.size(), 1024 - 256);

    map.SetIndexRange(0, 256);
    ASSERT_EQ(map.size(), 1024);

    for (uint32_t i = 0; i < 4; ++i) {
      map.RemoveIndexRange(256 * i, 256);
      ASSERT_EQ(map.size(), 1024 - 256 * (i + 1));
    }
    for (uint32_t i = 0; i < 4; ++i) {
      map.SetIndexRange(256 * i, 256);
      ASSERT_EQ(map.size(), 256 * (i + 1));
    }
    map.Clear();
    ASSERT_EQ(map.size(), 0);
  }
}
