// Copyright (c) 2025-2026 Juantgd. All Rights Reserved.

#ifndef JDOCS_CORE_TIMER_H_
#define JDOCS_CORE_TIMER_H_

#include <cstdint>
#include <functional>

namespace jdocs {

namespace {

#define TIME_WHEEL_TVR_BITS 8
#define TIME_WHEEL_TVR_SIZE (1 << TIME_WHEEL_TVR_BITS)
#define TIME_WHEEL_TVR_MASK TIME_WHEEL_TVR_SIZE - 1
#define TIME_WHEEL_TVN_BITS 6
#define TIME_WHEEL_TVN_SIZE (1 << TIME_WHEEL_TVN_BITS)
#define TIME_WHEEL_TVN_MASK (TIME_WHEEL_TVN_SIZE - 1)

} // namespace

class TimeWheel {
public:
  using TimerCallBack = std::function<void()>;
  // 时间轮间隔，单位毫秒
  TimeWheel(uint32_t tick = 100);
  ~TimeWheel() = default;

  TimeWheel(const TimeWheel &) = delete;
  TimeWheel &operator=(const TimeWheel &) = delete;

  struct timer_node {
    uint64_t expires_;
    TimerCallBack callback_;
    timer_node *next, **pprev;
    bool flag;
    timer_node(TimerCallBack callback)
        : expires_(0), callback_(std::move(callback)), next(nullptr),
          pprev(nullptr), flag(false) {}
    // 防止对象销毁后破坏链表结构
    ~timer_node() {
      if (pprev)
        *pprev = next;
      if (next)
        next->pprev = pprev;
    }
    timer_node(const timer_node &) = delete;
    timer_node &operator=(const timer_node &) = delete;

    inline bool IsFired() const { return flag; }
  };

  static void timer_cancel(timer_node *timer);

  // 添加一个n毫秒的计时器
  void AddTimer(timer_node *timer, uint32_t millis);

  void Tick();

  void Update();

private:
  struct timer_head {
    timer_node *first{nullptr};
  };

  void __timer_add(timer_node *timer);

  void __timer_cascade(timer_head *tv, size_t index);

  inline static void __insert(timer_head *head, timer_node *timer) {
    timer->next = head->first;
    if (head->first) {
      head->first->pprev = &timer->next;
    }
    timer->pprev = &head->first;
    head->first = timer;
  }

  timer_head tv1[TIME_WHEEL_TVR_SIZE];
  timer_head tv2[TIME_WHEEL_TVN_SIZE];
  timer_head tv3[TIME_WHEEL_TVN_SIZE];
  timer_head tv4[TIME_WHEEL_TVN_SIZE];
  timer_head tv5[TIME_WHEEL_TVN_SIZE];

  uint64_t current_tick_{0};
  uint64_t start_millis_;
  uint32_t tick_;
};

} // namespace jdocs

#endif
