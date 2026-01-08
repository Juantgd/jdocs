// Copyright (c) 2025-2026 Juantgd. All Rights Reserved.

#include "core/timer.h"

#include <gtest/gtest.h>

TEST(TimerTest, TimerBasicTest) {
  using namespace jdocs;
  {
    TimeWheel tw;
    int count = 0;
    TimeWheel::timer_node timer1([&count]() { ++count; });
    TimeWheel::timer_node timer2([&count]() { ++count; });
    // 100ms后触发,即调用1次Tick触发
    tw.AddTimer(&timer1, 100);
    tw.AddTimer(&timer2, 100);
    ASSERT_EQ(count, 0);
    tw.Tick();
    ASSERT_EQ(count, 2);

    ASSERT_EQ(timer1.IsFired(), true);
    ASSERT_EQ(timer2.IsFired(), true);
  }
  {
    TimeWheel tw;
    int count = 0;
    TimeWheel::timer_node timer1([&count]() { ++count; });
    TimeWheel::timer_node timer2([&count]() { ++count; });
    TimeWheel::timer_node timer3([&count]() { ++count; });
    // 1s后触发,即调用10次Tick触发
    tw.AddTimer(&timer1, 1000);
    // 假设过了500ms
    tw.Tick();
    tw.Tick();
    tw.Tick();
    tw.Tick();
    tw.Tick();
    // 添加一个500ms后触发的超时事件，即和timer1同时触发
    tw.AddTimer(&timer2, 500);
    ASSERT_EQ(count, 0);
    // 200ms后
    tw.Tick();
    tw.Tick();
    // 添加一个300ms后触发的超时事件，即和timer1,timer2同时触发
    tw.AddTimer(&timer3, 300);
    ASSERT_EQ(count, 0);
    tw.Tick();
    tw.Tick();
    tw.Tick();
    ASSERT_EQ(timer1.IsFired(), true);
    ASSERT_EQ(timer2.IsFired(), true);
    ASSERT_EQ(timer3.IsFired(), true);
    ASSERT_EQ(count, 3);
  }
  {
    // 10ms误差
    TimeWheel tw(10);
    int count = 0;
    TimeWheel::timer_node t1([&]() { ++count; });
    TimeWheel::timer_node t2([&]() { ++count; });
    TimeWheel::timer_node t3([&]() { ++count; });
    // 2.56 秒后触发，即256次tick
    tw.AddTimer(&t1, 2560);
    tw.AddTimer(&t2, 2570);
    tw.AddTimer(&t3, 2580);
    for (int i = 0; i < 256; ++i) {
      tw.Tick();
    }
    ASSERT_EQ(count, 1);
    tw.Tick();
    ASSERT_EQ(count, 2);
    tw.Tick();
    ASSERT_EQ(count, 3);
  }
}
